#import "J2MEHostViewController.h"
#import "J2MEScreenView.h"
#import "J2MESettings.h"
#import "J2MESettingsViewController.h"
#import "J2MELibraryViewController.h"
#import "J2MEBridge.h"

#include "j2me_kernel.h"

NSString *const J2MEDidReceiveJarURLNotification = @"J2MEDidReceiveJarURLNotification";
NSString *const J2MEJarURLKey = @"J2MEJarURLKey";

static CGFloat const kToolbarHeight = 52.0;

@interface J2MEHostViewController () <J2MELibraryDelegate> {
    J2MEScreenView *_screenView;
    UIToolbar      *_toolbar;
    UILabel        *_statusLabel;
    BOOL            _immersive;
    NSString       *_jarDirectory;
    NSString       *_homeDirectory;
    NSString       *_resourceDirectory;
}
@end

@implementation J2MEHostViewController

#pragma mark - 生命周期

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.07 green:0.08 blue:0.11 alpha:1.0];
    self.title = @"J2ME";
    [self prepareDirectories];

    _screenView = [[J2MEScreenView alloc] initWithFrame:CGRectZero];
    _screenView.scaleMode = [J2MESettings scaleMode];
    _screenView.filterMode = [J2MESettings filterMode];
    [self.view addSubview:_screenView];

    [self buildNavigationBar];
    [self buildToolbar];

    [J2MEBridge shared].screenView = _screenView;
    [self startKernel];

    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(settingsChanged:) name:J2MESettingsDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(exitRequested:) name:J2MEDidRequestExitNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(receiveJarURL:) name:J2MEDidReceiveJarURLNotification object:nil];
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)prepareDirectories {
    NSString *documents = NSSearchPathForDirectoriesInDomains(
        NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *support = NSSearchPathForDirectoriesInDomains(
        NSApplicationSupportDirectory, NSUserDomainMask, YES).firstObject;
    NSFileManager *fm = [NSFileManager defaultManager];
    _jarDirectory = [documents stringByAppendingPathComponent:@"J2ME"];
    _homeDirectory = [support stringByAppendingPathComponent:@"J2ME"];
    _resourceDirectory = [[NSBundle mainBundle] resourcePath] ?: @"";
    [fm createDirectoryAtPath:_jarDirectory withIntermediateDirectories:YES
                   attributes:nil error:nil];
    [fm createDirectoryAtPath:[_homeDirectory stringByAppendingPathComponent:@"rms"]
  withIntermediateDirectories:YES attributes:nil error:nil];
    [fm createDirectoryAtPath:[_homeDirectory stringByAppendingPathComponent:@"appdb"]
  withIntermediateDirectories:YES attributes:nil error:nil];
}

- (void)startKernel {
    J2MEBridge *bridge = [J2MEBridge shared];
    BOOL ok = [bridge startWithResourceDir:_resourceDirectory
                                  homeDir:_homeDirectory
                             documentsDir:_jarDirectory];
    if (!ok) {
        [self showMessage:@"内核初始化失败" title:@"错误"];
        return;
    }
    [bridge setVolume:[J2MESettings masterVolume]];
    [bridge setKeypadVisible:[J2MESettings keypadVisibleByDefault]];
}

- (void)buildNavigationBar {
    self.navigationItem.leftBarButtonItem =
        [[UIBarButtonItem alloc] initWithTitle:@"游戏库"
                                         style:UIBarButtonItemStylePlain
                                        target:self action:@selector(openLibrary:)];
    self.navigationItem.rightBarButtonItem =
        [[UIBarButtonItem alloc] initWithTitle:@"设置"
                                         style:UIBarButtonItemStylePlain
                                        target:self action:@selector(openSettings:)];
    if (self.navigationController) {
        UINavigationBarAppearance *appearance = [[UINavigationBarAppearance alloc] init];
        [appearance configureWithOpaqueBackground];
        appearance.backgroundColor = [UIColor colorWithRed:0.11 green:0.13 blue:0.17 alpha:1.0];
        appearance.titleTextAttributes = @{NSForegroundColorAttributeName: [UIColor whiteColor]};
        self.navigationController.navigationBar.standardAppearance = appearance;
        self.navigationController.navigationBar.scrollEdgeAppearance = appearance;
        self.navigationController.navigationBar.tintColor = [UIColor colorWithRed:0.45 green:0.72 blue:1.0 alpha:1.0];
    }
}

- (void)buildToolbar {
    _toolbar = [[UIToolbar alloc] initWithFrame:CGRectZero];
    _toolbar.barStyle = UIBarStyleBlack;
    _toolbar.translucent = YES;
    _toolbar.tintColor = [UIColor colorWithRed:0.55 green:0.78 blue:1.0 alpha:1.0];
    UIBarButtonItem *flex = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:nil action:nil];
    UIBarButtonItem *library = [[UIBarButtonItem alloc]
        initWithTitle:@"选游戏" style:UIBarButtonItemStylePlain
               target:self action:@selector(openLibrary:)];
    UIBarButtonItem *keypad = [[UIBarButtonItem alloc]
        initWithTitle:@"键盘" style:UIBarButtonItemStylePlain
               target:self action:@selector(toggleKeypad:)];
    UIBarButtonItem *scale = [[UIBarButtonItem alloc]
        initWithTitle:@"缩放" style:UIBarButtonItemStylePlain
               target:self action:@selector(cycleScale:)];
    UIBarButtonItem *shot = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemAction
               target:self action:@selector(shareScreenshot:)];
    _toolbar.items = @[library, flex, keypad, flex, scale, flex, shot];
    [self.view addSubview:_toolbar];
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    CGRect bounds = self.view.bounds;
    UIEdgeInsets insets = self.view.safeAreaInsets;
    CGFloat width = bounds.size.width - insets.left - insets.right;
    CGFloat top = insets.top;
    CGFloat bottomInset = insets.bottom;
    CGFloat toolbarHeight = _immersive ? 0.0 : kToolbarHeight;
    CGFloat availableHeight = bounds.size.height - top - bottomInset - toolbarHeight;
    if (availableHeight < 0) availableHeight = 0;

    _screenView.frame = CGRectMake(insets.left, top, width, availableHeight);
    _toolbar.frame = CGRectMake(0,
        bounds.size.height - bottomInset - toolbarHeight,
        bounds.size.width, toolbarHeight);
    _toolbar.hidden = _immersive;
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    BOOL landscape = size.width > size.height;
    __weak typeof(self) weakSelf = self;
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> ctx) {
        __strong typeof(weakSelf) self = weakSelf;
        if (!self) return;
        self->_immersive = landscape;
        [self.navigationController setNavigationBarHidden:landscape animated:YES];
        [self.view setNeedsLayout];
        [self.view layoutIfNeeded];
        [self setNeedsStatusBarAppearanceUpdate];
    } completion:nil];
}

- (BOOL)prefersStatusBarHidden {
    return _immersive;
}

- (UIStatusBarStyle)preferredStatusBarStyle {
    return UIStatusBarStyleLightContent;
}

#pragma mark - 通知与设置

- (void)settingsChanged:(NSNotification *)note {
    (void)note;
    _screenView.scaleMode = [J2MESettings scaleMode];
    _screenView.filterMode = [J2MESettings filterMode];
    [_screenView setNeedsLayout];
}

- (void)exitRequested:(NSNotification *)note {
    (void)note;
    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:@"退出游戏？"
                         message:@"对应 BBK 9588 原项目的「是/否」确认框。选择「是」将销毁 MIDlet 并安全退出运行时。"
                  preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"否"
        style:UIAlertActionStyleCancel handler:^(UIAlertAction *action) {
            (void)action;
            /* 原项目：选择「否」后重绘浮层，面板外触摸继续传给游戏 */
            [[J2MEBridge shared] setKeypadVisible:YES];
        }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"是"
        style:UIAlertActionStyleDestructive handler:^(UIAlertAction *action) {
            (void)action;
            [self returnToSystemMenu];
        }]];
    [self presentViewController:alert animated:YES completion:nil];
}

- (void)returnToSystemMenu {
    J2MEBridge *bridge = [J2MEBridge shared];
    [bridge stop];
    [self startKernel];
    [self openLibrary:nil];
}

- (void)receiveJarURL:(NSNotification *)note {
    NSURL *url = note.userInfo[J2MEJarURLKey];
    if (![url isKindOfClass:[NSURL class]])
        return;
    J2MELibraryViewController *library = [[J2MELibraryViewController alloc]
                                          initWithStyle:UITableViewStylePlain];
    library.jarDirectory = _jarDirectory;
    NSError *error = nil;
    NSString *path = [library importJarFromURL:url error:&error];
    if (path != nil) {
        [self launchJarAtPath:path];
    } else {
        [self showMessage:error.localizedDescription ?: @"导入失败" title:@"导入失败"];
    }
}

#pragma mark - 动作

- (void)openLibrary:(id)sender {
    (void)sender;
    J2MELibraryViewController *library = [[J2MELibraryViewController alloc]
                                          initWithStyle:UITableViewStylePlain];
    library.jarDirectory = _jarDirectory;
    library.delegate = self;
    UINavigationController *nav = [[UINavigationController alloc]
                                   initWithRootViewController:library];
    nav.modalPresentationStyle = UIModalPresentationFormSheet;
    [self presentViewController:nav animated:YES completion:nil];
}

- (void)openSettings:(id)sender {
    (void)sender;
    J2MESettingsViewController *settings = [[J2MESettingsViewController alloc]
                                           initWithStyle:UITableViewStyleGrouped];
    UINavigationController *nav = [[UINavigationController alloc]
                                   initWithRootViewController:settings];
    nav.modalPresentationStyle = UIModalPresentationFormSheet;
    [self presentViewController:nav animated:YES completion:nil];
}

- (void)toggleKeypad:(id)sender {
    (void)sender;
    J2MEBridge *bridge = [J2MEBridge shared];
    [bridge setKeypadVisible:![bridge keypadVisible]];
}

- (void)cycleScale:(id)sender {
    (void)sender;
    J2MEScaleMode next = ([J2MESettings scaleMode] + 1) % 3;
    [J2MESettings setScaleMode:next];
    NSArray *names = @[@"像素完美", @"等比适配", @"拉伸填满"];
    [self showMessage:[NSString stringWithFormat:@"缩放模式：%@", names[next]]
                title:@"显示"];
}

- (void)shareScreenshot:(id)sender {
    UIImage *image = [_screenView snapshot];
    if (image == nil)
        return;
    UIActivityViewController *vc = [[UIActivityViewController alloc]
        initWithActivityItems:@[image] applicationActivities:nil];
    vc.popoverPresentationController.barButtonItem =
        (UIBarButtonItem *)sender;
    [self presentViewController:vc animated:YES completion:nil];
}

- (void)launchJarAtPath:(NSString *)path {
    NSError *error = nil;
    if (![[J2MEBridge shared] loadJarAtPath:path error:&error]) {
        [self showMessage:error.localizedDescription ?: @"启动失败" title:@"错误"];
        return;
    }
    [[J2MEBridge shared] setKeypadVisible:[J2MESettings keypadVisibleByDefault]];
}

- (void)showMessage:(NSString *)message title:(NSString *)title {
    UIAlertController *alert = [UIAlertController
        alertControllerWithTitle:title message:message
                  preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"好"
                                              style:UIAlertActionStyleDefault
                                            handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
}

#pragma mark - J2MELibraryDelegate

- (void)library:(J2MELibraryViewController *)library didSelectJarAtPath:(NSString *)path {
    (void)library;
    [self launchJarAtPath:path];
}

#pragma mark - 外接键盘 / 手柄（可选优化）

static int KeyCodeForCharacters(NSString *characters) {
    if (characters.length == 0) return 0;
    unichar c = [characters characterAtIndex:0];
    if (c >= '0' && c <= '9') return (int)c;
    if (c == '*' || c == '#') return (int)c;
    switch (c) {
        case 'w': case 'W': return J2ME_KEY_UP;
        case 's': case 'S': return J2ME_KEY_DOWN;
        case 'a': case 'A': return J2ME_KEY_LEFT;
        case 'd': case 'D': return J2ME_KEY_RIGHT;
        case ' ': case '\r': case '\n': return J2ME_KEY_FIRE;
        default: break;
    }
    if ([characters isEqualToString:UIKeyInputUpArrow])    return J2ME_KEY_UP;
    if ([characters isEqualToString:UIKeyInputDownArrow])  return J2ME_KEY_DOWN;
    if ([characters isEqualToString:UIKeyInputLeftArrow])  return J2ME_KEY_LEFT;
    if ([characters isEqualToString:UIKeyInputRightArrow]) return J2ME_KEY_RIGHT;
    if ([characters isEqualToString:UIKeyInputEscape])     return -1000;
    return 0;
}

- (void)pressesBegan:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
    (void)event;
    for (UIPress *press in presses) {
        int key = KeyCodeForCharacters(press.key.characters);
        if (key == -1000) {
            [[NSNotificationCenter defaultCenter]
                postNotificationName:J2MEDidRequestExitNotification object:nil];
        } else if (key != 0) {
            [[J2MEBridge shared] postKeyType:J2ME_INPUT_KEY_DOWN key:key];
        }
    }
}

- (void)pressesEnded:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
    (void)event;
    for (UIPress *press in presses) {
        int key = KeyCodeForCharacters(press.key.characters);
        if (key == 0 || key == -1000)
            continue;
        [[J2MEBridge shared] postKeyType:J2ME_INPUT_KEY_UP key:key];
    }
}

@end
