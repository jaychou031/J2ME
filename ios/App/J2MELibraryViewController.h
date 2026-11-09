#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@class J2MELibraryViewController;

@protocol J2MELibraryDelegate <NSObject>
- (void)library:(J2MELibraryViewController *)library
didSelectJarAtPath:(NSString *)path;
@end

/* JAR 库：对应原项目 B:\J2ME\（无 B: 时 A:\J2ME\）的游戏选择器目录。
 * iOS 上用 App 的 Documents/J2ME/ 承担，并通过「文件」共享 / 分享面板导入。 */
@interface J2MELibraryViewController : UITableViewController

@property (nonatomic, weak) id<J2MELibraryDelegate> delegate;
@property (nonatomic, copy) NSString *jarDirectory;

- (void)reload;
/* 返回导入后的目标路径，失败返回 nil */
- (nullable NSString *)importJarFromURL:(NSURL *)url error:(NSError **)error;

@end

NS_ASSUME_NONNULL_END
