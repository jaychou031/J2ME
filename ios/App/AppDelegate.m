#import "AppDelegate.h"
#import "J2MEHostViewController.h"

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application
didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    (void)launchOptions;
    /* Info.plist 未声明 UIApplicationSceneManifest，走 AppDelegate 生命周期，
     * 这样可以省掉 SceneDelegate，减少无 Mac 环境下的维护面。 */
    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    self.window.backgroundColor = [UIColor blackColor];

    J2MEHostViewController *host = [[J2MEHostViewController alloc] init];
    UINavigationController *nav = [[UINavigationController alloc]
                                   initWithRootViewController:host];
    self.window.rootViewController = nav;
    [self.window makeKeyAndVisible];

    /* 冷启动若是被「文件」App 分享 .jar 唤起 */
    NSURL *url = launchOptions[UIApplicationLaunchOptionsURLKey];
    if ([url isKindOfClass:[NSURL class]]) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                     (int64_t)(0.6 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            [self handleIncomingJarURL:url];
        });
    }
    return YES;
}

/* 从「文件」App / 其他 App 通过「共享」打开 .jar */
- (BOOL)application:(UIApplication *)application
            openURL:(NSURL *)url
            options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options {
    (void)application; (void)options;
    return [self handleIncomingJarURL:url];
}

- (BOOL)handleIncomingJarURL:(NSURL *)url {
    if (![url.pathExtension.lowercaseString isEqualToString:@"jar"])
        return NO;
    BOOL accessing = [url startAccessingSecurityScopedResource];
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *name = url.lastPathComponent ?: @"game.jar";
    NSString *temp = [NSTemporaryDirectory() stringByAppendingPathComponent:name];
    [fm removeItemAtPath:temp error:nil];
    NSError *error = nil;
    BOOL ok = [fm copyItemAtURL:url toURL:[NSURL fileURLWithPath:temp] error:&error];
    if (accessing)
        [url stopAccessingSecurityScopedResource];
    if (!ok)
        return NO;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:J2MEDidReceiveJarURLNotification
                      object:nil
                    userInfo:@{J2MEJarURLKey: [NSURL fileURLWithPath:temp]}];
    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {
    (void)application;
    /* 沿用原项目：进入后台不做特殊 VM 操作，由系统挂起 */
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    (void)application;
}

@end
