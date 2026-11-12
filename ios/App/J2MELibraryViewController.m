#import "J2MELibraryViewController.h"
#import "J2MEBridge.h"

#include "ios_platform.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <MobileCoreServices/MobileCoreServices.h>

@interface J2MELibraryViewController () <UIDocumentPickerDelegate> {
    NSArray<NSString *> *_files;
    NSMutableDictionary<NSString *, NSString *> *_midletCache;
}
@end

@implementation J2MELibraryViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"游戏库";
    self.tableView.backgroundColor = [UIColor colorWithRed:0.07 green:0.08 blue:0.11 alpha:1.0];
    self.tableView.separatorColor = [UIColor colorWithWhite:0.18 alpha:1.0];
    self.navigationItem.leftBarButtonItem =
        [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                      target:self
                                                      action:@selector(dismiss:)];
    self.navigationItem.rightBarButtonItem =
        [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAdd
                                                      target:self
                                                      action:@selector(importTapped:)];
    _midletCache = [NSMutableDictionary dictionary];
    [self reload];
}

- (void)dismiss:(id)sender {
    (void)sender;
    [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)reload {
    NSFileManager *fm = [NSFileManager defaultManager];
    NSMutableArray *found = [NSMutableArray array];
    NSArray<NSString *> *items = [fm contentsOfDirectoryAtPath:self.jarDirectory error:nil];
    for (NSString *name in items) {
        if ([name.pathExtension.lowercaseString isEqualToString:@"jar"])
            [found addObject:name];
    }
    [found sortUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
    _files = [found copy];
    [self.tableView reloadData];
    if (_files.count == 0) {
        UILabel *empty = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, 300, 120)];
        empty.numberOfLines = 0;
        empty.textAlignment = NSTextAlignmentCenter;
        empty.textColor = [UIColor colorWithWhite:0.6 alpha:1.0];
        empty.font = [UIFont systemFontOfSize:15];
        empty.text = @"还没有 JAR 游戏\n\n点右上角 + 从「文件」导入 .jar\n"
                     @"或用 iTunes / Finder 文件共享放到\nDocuments/J2ME/ 目录";
        self.tableView.tableFooterView = empty;
    } else {
        self.tableView.tableFooterView = [UIView new];
    }
}

- (NSString *)midletForFile:(NSString *)name {
    NSString *cached = _midletCache[name];
    if (cached != nil)
        return cached;
    NSString *path = [self.jarDirectory stringByAppendingPathComponent:name];
    IOSJar jar;
    NSString *result = @"(Manifest 不可读)";
    if (ios_jar_open(path.fileSystemRepresentation, &jar, 0) == 0) {
        if (jar.midlet1[0] != '\0')
            result = [NSString stringWithUTF8String:jar.midlet1] ?: result;
        else
            result = @"(未声明 MIDlet-1)";
        ios_jar_close(&jar);
    }
    _midletCache[name] = result;
    return result;
}

#pragma mark - 导入

- (void)importTapped:(id)sender {
    (void)sender;
    // UniformTypeIdentifiers 并没有导出 UTTypeZIPArchive 这个符号（iOS 15
    // SDK 上直接报 use of undeclared identifier）；旧 MobileCoreServices 的
    // kUTTypeZipArchive 也已废弃。统一按 UTI 字符串构造 UTType，零常量依赖。
    UTType *jarType  = [UTType typeWithIdentifier:@"cn.bbkclyde.j2me.jar"];
    UTType *zipType  = [UTType typeWithIdentifier:@"public.zip-archive"];
    UTType *dataType = [UTType typeWithIdentifier:@"public.data"];
    NSMutableArray<UTType *> *types = [NSMutableArray array];
    if (jarType)  [types addObject:jarType];   /* Info.plist 里导出的自定义 .jar UTI */
    if (zipType)  [types addObject:zipType];   /* JAR 本质是 zip 归档 */
    if (dataType) [types addObject:dataType];  /* 兜底：部分系统不认识 .jar 的 UTI */
    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:types asCopy:YES];
    picker.delegate = self;
    picker.allowsMultipleSelection = YES;
    picker.modalPresentationStyle = UIModalPresentationFormSheet;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller
didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    (void)controller;
    for (NSURL *url in urls) {
        NSError *error = nil;
        if ([self importJarFromURL:url error:&error] == nil) {
            UIAlertController *alert = [UIAlertController
                alertControllerWithTitle:@"导入失败"
                                 message:error.localizedDescription ?: @"未知错误"
                          preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:@"好"
                                                      style:UIAlertActionStyleDefault
                                                    handler:nil]];
            [self presentViewController:alert animated:YES completion:nil];
        }
    }
    [self reload];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {
    (void)controller;
}

- (NSString *)importJarFromURL:(NSURL *)url error:(NSError **)error {
    if (![url.pathExtension.lowercaseString isEqualToString:@"jar"]) {
        if (error)
            *error = [NSError errorWithDomain:@"J2ME" code:-10
                      userInfo:@{NSLocalizedDescriptionKey: @"只支持 .jar 文件"}];
        return nil;
    }
    NSFileManager *fm = [NSFileManager defaultManager];
    [fm createDirectoryAtPath:self.jarDirectory
  withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *name = url.lastPathComponent ?: @"game.jar";
    NSString *dest = [self.jarDirectory stringByAppendingPathComponent:name];
    if ([fm fileExistsAtPath:dest]) {
        NSString *stem = [name stringByDeletingPathExtension];
        NSUInteger index = 2;
        while ([fm fileExistsAtPath:dest]) {
            dest = [self.jarDirectory stringByAppendingPathComponent:
                    [NSString stringWithFormat:@"%@-%lu.jar",
                     stem, (unsigned long)index++]];
        }
    }
    [fm removeItemAtPath:dest error:nil];
    BOOL ok = [fm moveItemAtURL:url toURL:[NSURL fileURLWithPath:dest] error:error];
    if (!ok && error && *error == nil)
        *error = [NSError errorWithDomain:@"J2ME" code:-11
                  userInfo:@{NSLocalizedDescriptionKey: @"无法移动文件"}];
    if (!ok)
        return nil;
    [_midletCache removeAllObjects];
    [[J2MEBridge shared] appendLog:
        [NSString stringWithFormat:@"导入 %@", dest.lastPathComponent]];
    return dest;
}

#pragma mark - Table view

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    (void)tableView;
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    (void)tableView; (void)section;
    return (NSInteger)_files.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    static NSString *identifier = @"JarCell";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:identifier];
    if (cell == nil)
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                      reuseIdentifier:identifier];
    NSString *name = _files[(NSUInteger)indexPath.row];
    NSString *path = [self.jarDirectory stringByAppendingPathComponent:name];
    NSDictionary *attrs = [[NSFileManager defaultManager]
                           attributesOfItemAtPath:path error:nil];
    unsigned long long bytes = [attrs fileSize];
    cell.backgroundColor = [UIColor colorWithRed:0.10 green:0.12 blue:0.16 alpha:1.0];
    cell.textLabel.text = name;
    cell.textLabel.textColor = [UIColor whiteColor];
    cell.detailTextLabel.textColor = [UIColor colorWithWhite:0.62 alpha:1.0];
    cell.detailTextLabel.text = [NSString stringWithFormat:
        @"%.1f KiB   ·   %@", (double)bytes / 1024.0, [self midletForFile:name]];
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    NSString *path = [self.jarDirectory
                      stringByAppendingPathComponent:_files[(NSUInteger)indexPath.row]];
    if ([self.delegate respondsToSelector:@selector(library:didSelectJarAtPath:)])
        [self.delegate library:self didSelectJarAtPath:path];
    [self dismissViewControllerAnimated:YES completion:nil];
}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath {
    (void)tableView; (void)indexPath;
    return YES;
}

- (void)tableView:(UITableView *)tableView
commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
forRowAtIndexPath:(NSIndexPath *)indexPath {
    if (editingStyle != UITableViewCellEditingStyleDelete)
        return;
    NSString *name = _files[(NSUInteger)indexPath.row];
    NSString *path = [self.jarDirectory stringByAppendingPathComponent:name];
    [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
    [_midletCache removeObjectForKey:name];
    [self reload];
}

@end
