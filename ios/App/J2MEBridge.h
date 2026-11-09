#import <Foundation/Foundation.h>

@class J2MEScreenView;

NS_ASSUME_NONNULL_BEGIN

/* 软键盘 X 键（原项目的 SDK bda_confirm 退出确认）由宿主弹窗处理 */
extern NSString *const J2MEDidRequestExitNotification;
extern NSString *const J2MEDidChangeStateNotification;
extern NSString *const J2MEStateKey;

/* App 层与内核之间唯一的桥。所有 C ABI 回调都收敛到这里，
 * 再转发给 UIKit。CADisplayLink 在主线程驱动内核 pump()。 */
@interface J2MEBridge : NSObject

@property (nonatomic, weak) J2MEScreenView *screenView;
@property (nonatomic, readonly) NSArray<NSString *> *logLines;
@property (nonatomic, readonly, getter=isRunning) BOOL running;
@property (nonatomic, readonly) NSString *kernelVersion;

+ (instancetype)shared;

- (BOOL)startWithResourceDir:(NSString *)resourceDir
                     homeDir:(NSString *)homeDir
                documentsDir:(NSString *)documentsDir;
- (void)stop;

- (BOOL)loadJarAtPath:(NSString *)path error:(NSError **)error;
- (NSString *)diagnosticsText;

- (void)postPointerType:(int)type x:(int)x y:(int)y;
- (void)postKeyType:(int)type key:(int)key;

- (void)setVolume:(int)percent;
- (int)volume;
- (void)setMuted:(BOOL)muted;
- (void)setKeypadVisible:(BOOL)visible;
- (BOOL)keypadVisible;

- (void)appendLog:(NSString *)line;
- (void)clearLog;

@end

NS_ASSUME_NONNULL_END
