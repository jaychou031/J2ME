#import "J2MEBridge.h"
#import "J2MEScreenView.h"

#include "j2me_kernel.h"
#include "ios_platform.h"
#include <string.h>

#import <UIKit/UIKit.h>
#import <QuartzCore/CADisplayLink.h>
#import <QuartzCore/CoreAnimation.h>

NSString *const J2MEDidRequestExitNotification = @"J2MEDidRequestExitNotification";
NSString *const J2MEDidChangeStateNotification = @"J2MEDidChangeStateNotification";
NSString *const J2MEStateKey = @"J2MEStateKey";

static NSString *const kVolumeDefaultsKey = @"J2MEMasterVolume";

@interface J2MEBridge () {
    const J2MEKernel *_kernel;
    CADisplayLink    *_displayLink;
    char _home[1024];
    char _resource[1024];
    char _documents[1024];
    NSMutableArray<NSString *> *_logs;
    BOOL _running;
}
@end

/* ---------- C ABI 回调 ---------- */

static void J2MEFrameCallback(const uint16_t *fb, int w, int h, void *user) {
    J2MEBridge *bridge = (__bridge J2MEBridge *)user;
    J2MEScreenView *view = bridge.screenView;
    if (view != nil)
        [view setFrameBuffer:fb width:w height:h];
}

static void J2MELogCallback(const char *message, void *user) {
    J2MEBridge *bridge = (__bridge J2MEBridge *)user;
    if (message == NULL)
        return;
    [bridge appendLog:[NSString stringWithUTF8String:message]];
}

static void J2MEStateCallback(J2MEState state, void *user) {
    (void)user;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:J2MEDidChangeStateNotification
                      object:nil
                    userInfo:@{J2MEStateKey: @((int)state)}];
}

static void J2MEExitRequestCallback(void *user) {
    (void)user;
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter]
            postNotificationName:J2MEDidRequestExitNotification
                          object:nil];
    });
}

@implementation J2MEBridge

+ (instancetype)shared {
    static J2MEBridge *instance;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ instance = [[J2MEBridge alloc] init]; });
    return instance;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _kernel = j2me_kernel_get();
        _logs = [NSMutableArray array];
        _running = NO;
        memset(_home, 0, sizeof(_home));
        memset(_resource, 0, sizeof(_resource));
        memset(_documents, 0, sizeof(_documents));
    }
    return self;
}

- (NSString *)kernelVersion {
    const char *v = _kernel->version ? _kernel->version() : "unknown";
    return [NSString stringWithUTF8String:v] ?: @"unknown";
}

- (BOOL)startWithResourceDir:(NSString *)resourceDir
                     homeDir:(NSString *)homeDir
                documentsDir:(NSString *)documentsDir {
    if (_kernel == NULL)
        return NO;
    [self appendLog:[NSString stringWithFormat:@"内核：%@", self.kernelVersion]];

    strncpy(_home, homeDir.fileSystemRepresentation, sizeof(_home) - 1);
    strncpy(_resource, resourceDir.fileSystemRepresentation, sizeof(_resource) - 1);
    strncpy(_documents, documentsDir.fileSystemRepresentation, sizeof(_documents) - 1);
    _home[sizeof(_home) - 1] = '\0';
    _resource[sizeof(_resource) - 1] = '\0';
    _documents[sizeof(_documents) - 1] = '\0';

    NSInteger saved = [[NSUserDefaults standardUserDefaults]
                       integerForKey:kVolumeDefaultsKey];
    int volume = saved > 0 ? (int)saved : 100;

    J2MEConfig config;
    memset(&config, 0, sizeof(config));
    config.homeDir = _home;
    config.resourceDir = _resource;
    config.documentsDir = _documents;
    config.javaHeapBytes = J2ME_DEFAULT_JAVA_HEAP_BYTES;
    config.allocLimitBytes = J2ME_DEFAULT_ALLOC_LIMIT;
    config.masterVolume = volume;

    J2MEHostCallbacks host;
    memset(&host, 0, sizeof(host));
    host.onFrame = J2MEFrameCallback;
    host.onPcm = NULL;              /* 内核直接走 ios_audio -> AudioQueue */
    host.onLog = J2MELogCallback;
    host.onState = J2MEStateCallback;
    host.onExitRequest = J2MEExitRequestCallback;
    host.user = (__bridge void *)self;

    if (_kernel->init(&config, &host) != 0) {
        [self appendLog:@"内核初始化失败"];
        return NO;
    }

    _running = YES;
    _displayLink = [CADisplayLink displayLinkWithTarget:self
                                              selector:@selector(tick:)];
    if (@available(iOS 15.0, *)) {
        /* iOS 15 起用 frameRateRange，避免 preferredFramesPerSecond 弃用警告 */
        _displayLink.preferredFrameRateRange = CAFrameRateRangeMake(60, 60, 60);
    } else {
        _displayLink.preferredFramesPerSecond = 60;
    }
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                       forMode:NSRunLoopCommonModes];
    [self appendLog:@"CADisplayLink 60fps 已启动"];
    return YES;
}

- (void)tick:(CADisplayLink *)link {
    (void)link;
    if (_kernel == NULL || !_running)
        return;
    int rc = _kernel->pump();
    if (rc < 0) {
        /* phoneME 内核主动退出（SHUTDOWN_EVENT） */
        [self appendLog:@"MIDlet 已请求退出"];
        [[NSNotificationCenter defaultCenter]
            postNotificationName:J2MEDidRequestExitNotification
                          object:nil];
    }
}

- (BOOL)loadJarAtPath:(NSString *)path error:(NSError **)error {
    if (_kernel == NULL) {
        if (error)
            *error = [NSError errorWithDomain:@"J2ME" code:-1
                      userInfo:@{NSLocalizedDescriptionKey: @"内核未初始化"}];
        return NO;
    }
    if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
        if (error)
            *error = [NSError errorWithDomain:@"J2ME" code:-2
                      userInfo:@{NSLocalizedDescriptionKey: @"JAR 文件不存在"}];
        return NO;
    }
    int rc = _kernel->startJar(path.fileSystemRepresentation);
    if (rc != 0) {
        if (error)
            *error = [NSError errorWithDomain:@"J2ME" code:rc
                      userInfo:@{NSLocalizedDescriptionKey: @"内核启动 JAR 失败"}];
        return NO;
    }
    [self appendLog:[NSString stringWithFormat:@"启动 %@", path.lastPathComponent]];
    return YES;
}

- (NSString *)diagnosticsText {
    if (_kernel == NULL || _kernel->diagnostics == NULL)
        return @"(无诊断信息)";
    const char *text = _kernel->diagnostics();
    return [NSString stringWithUTF8String:text] ?: @"(诊断信息编码异常)";
}

- (void)stop {
    if (_displayLink != nil) {
        [_displayLink invalidate];
        _displayLink = nil;
    }
    if (_kernel != NULL && _running) {
        _kernel->shutdown();
        [self appendLog:@"内核已停止"];
    }
    _running = NO;
}

- (void)postPointerType:(int)type x:(int)x y:(int)y {
    if (_kernel != NULL && _running)
        _kernel->postInput((J2MEInputType)type, x, y);
}

- (void)postKeyType:(int)type key:(int)key {
    if (_kernel != NULL && _running)
        _kernel->postInput((J2MEInputType)type, key, 0);
}

- (void)setVolume:(int)percent {
    if (_kernel != NULL)
        _kernel->setVolume(percent);
    [[NSUserDefaults standardUserDefaults] setInteger:percent
                                              forKey:kVolumeDefaultsKey];
}

- (int)volume {
    return _kernel != NULL ? _kernel->volume() : 100;
}

- (void)setMuted:(BOOL)muted {
    if (_kernel != NULL)
        _kernel->setMuted(muted ? 1 : 0);
}

- (void)setKeypadVisible:(BOOL)visible {
    if (_kernel != NULL)
        _kernel->setKeypadVisible(visible ? 1 : 0);
}

- (BOOL)keypadVisible {
    return _kernel != NULL && _kernel->keypadVisible() != 0;
}

- (NSArray<NSString *> *)logLines {
    return [_logs copy];
}

- (void)appendLog:(NSString *)line {
    if (line.length == 0)
        return;
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self appendLog:line]; });
        return;
    }
    NSDateFormatter *fmt = [[NSDateFormatter alloc] init];
    fmt.dateFormat = @"HH:mm:ss";
    [_logs addObject:[NSString stringWithFormat:@"[%@] %@",
                      [fmt stringFromDate:[NSDate date]], line]];
    if (_logs.count > 300)
        [_logs removeObjectsInRange:NSMakeRange(0, _logs.count - 300)];
}

- (void)clearLog {
    [_logs removeAllObjects];
}

@end
