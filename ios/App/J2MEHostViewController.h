#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString *const J2MEDidReceiveJarURLNotification;
extern NSString *const J2MEJarURLKey;

/* 主容器：把 240×320 画面按 iPhone 屏幕缩放居中，上下用原生 UIKit 工具栏
 * 提供「选游戏 / 键盘 / 缩放 / 截图 / 设置」。横屏时自动进入沉浸式全屏。 */
@interface J2MEHostViewController : UIViewController
@end

NS_ASSUME_NONNULL_END
