#import "J2MESettingsViewController.h"
#import "J2MESettings.h"
#import "J2MEBridge.h"
#import <math.h>

/* 通用只读文本页：用于显示 diagnostics（对应原项目 J2MEPHONE.LOG）与运行日志 */
@interface J2METextViewController : UIViewController
@property (nonatomic, copy) NSString *text;
@end

@implementation J2METextViewController {
    UITextView *_textView;
}
@synthesize text = _text;
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.07 green:0.08 blue:0.11 alpha:1.0];
    _textView = [[UITextView alloc] initWithFrame:self.view.bounds];
    _textView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _textView.backgroundColor = [UIColor clearColor];
    _textView.textColor = [UIColor colorWithWhite:0.85 alpha:1.0];
    _textView.font = [UIFont fontWithName:@"Menlo" size:12] ?: [UIFont systemFontOfSize:12];
    _textView.editable = NO;
    _textView.alwaysBounceVertical = YES;
    _textView.text = self.text;
    [self.view addSubview:_textView];
    self.navigationItem.rightBarButtonItem =
        [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAction
                                                      target:self
                                                      action:@selector(share:)];
}
- (void)setText:(NSString *)text {
    _text = [text copy];
    _textView.text = _text;
}
- (void)share:(id)sender {
    UIActivityViewController *vc =
        [[UIActivityViewController alloc] initWithActivityItems:@[self.text ?: @""]
                                          applicationActivities:nil];
    vc.popoverPresentationController.barButtonItem = (UIBarButtonItem *)sender;
    [self presentViewController:vc animated:YES completion:nil];
}
@end

@interface J2MESettingsViewController ()
@end

@implementation J2MESettingsViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"设置";
    self.tableView.backgroundColor = [UIColor colorWithRed:0.07 green:0.08 blue:0.11 alpha:1.0];
    self.tableView.separatorColor = [UIColor colorWithWhite:0.16 alpha:1.0];
    self.navigationItem.leftBarButtonItem =
        [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                      target:self
                                                      action:@selector(dismiss:)];
}

- (void)dismiss:(id)sender {
    (void)sender;
    [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - Table

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    (void)tableView;
    return 4;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    (void)tableView;
    switch (section) {
        case 0: return 2;   /* 缩放 / 插值 */
        case 1: return 1;   /* 键盘 */
        case 2: return 2;   /* 音量 / 静音 */
        case 3: return 3;   /* 版本 / 诊断 / 日志 */
        default: return 0;
    }
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
    (void)tableView;
    switch (section) {
        case 0: return @"显示（针对 iPhone 屏幕优化）";
        case 1: return @"悬浮软键盘";
        case 2: return @"音频";
        case 3: return @"内核与诊断";
        default: return nil;
    }
}

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
    (void)tableView;
    if (section == 0)
        return @"像素完美 = 整数倍最近邻放大（iPhone 13 竖屏通常 4×），最接近 9588 真机观感；等比适配可使用更多屏幕空间。";
    if (section == 3)
        return @"诊断计数与 BBK 9588 原项目 J2MEPHONE.LOG 的 SOFTPAD_* / RAW_* / AUDIO_* 一一对应。";
    return nil;
}

- (UITableViewCell *)cellWithIdentifier:(NSString *)identifier
                                  style:(UITableViewCellStyle)style {
    UITableViewCell *cell = [self.tableView dequeueReusableCellWithIdentifier:identifier];
    if (cell == nil)
        cell = [[UITableViewCell alloc] initWithStyle:style reuseIdentifier:identifier];
    cell.backgroundColor = [UIColor colorWithRed:0.10 green:0.12 blue:0.16 alpha:1.0];
    cell.textLabel.textColor = [UIColor whiteColor];
    cell.detailTextLabel.textColor = [UIColor colorWithWhite:0.6 alpha:1.0];
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    return cell;
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell;
    if (indexPath.section == 0 && indexPath.row == 0) {
        cell = [self cellWithIdentifier:@"Scale" style:UITableViewCellStyleDefault];
        cell.textLabel.text = nil;
        UISegmentedControl *seg = [[UISegmentedControl alloc] initWithItems:
                                   @[@"像素完美", @"等比适配", @"拉伸填满"]];
        seg.selectedSegmentIndex = (NSInteger)[J2MESettings scaleMode];
        seg.tag = 100;
        [seg addTarget:self action:@selector(segmentChanged:)
              forControlEvents:UIControlEventValueChanged];
        cell.accessoryView = seg;
        [self addLabel:@"缩放模式" toCell:cell];
    } else if (indexPath.section == 0 && indexPath.row == 1) {
        cell = [self cellWithIdentifier:@"Filter" style:UITableViewCellStyleDefault];
        UISegmentedControl *seg = [[UISegmentedControl alloc] initWithItems:
                                   @[@"最近邻", @"平滑"]];
        seg.selectedSegmentIndex = (NSInteger)[J2MESettings filterMode];
        seg.tag = 101;
        [seg addTarget:self action:@selector(segmentChanged:)
              forControlEvents:UIControlEventValueChanged];
        cell.accessoryView = seg;
        [self addLabel:@"插值" toCell:cell];
    } else if (indexPath.section == 1) {
        cell = [self cellWithIdentifier:@"Keypad" style:UITableViewCellStyleSubtitle];
        cell.textLabel.text = @"启动游戏时展开软键盘";
        cell.detailTextLabel.text = @"游戏内可点右上角悬浮点随时收起 / 展开";
        UISwitch *sw = [[UISwitch alloc] init];
        sw.on = [J2MESettings keypadVisibleByDefault];
        sw.tag = 200;
        [sw addTarget:self action:@selector(switchChanged:)
             forControlEvents:UIControlEventValueChanged];
        cell.accessoryView = sw;
    } else if (indexPath.section == 2 && indexPath.row == 0) {
        cell = [self cellWithIdentifier:@"Volume" style:UITableViewCellStyleDefault];
        UISlider *slider = [[UISlider alloc] initWithFrame:CGRectMake(0, 0, 170, 40)];
        slider.minimumValue = 0;
        slider.maximumValue = 100;
        slider.value = [J2MESettings masterVolume];
        slider.tag = 300;
        [slider addTarget:self action:@selector(sliderChanged:)
          forControlEvents:UIControlEventValueChanged];
        cell.accessoryView = slider;
        [self addLabel:[NSString stringWithFormat:@"主音量 %d%%",
                        [J2MESettings masterVolume]] toCell:cell];
    } else if (indexPath.section == 2 && indexPath.row == 1) {
        cell = [self cellWithIdentifier:@"Mute" style:UITableViewCellStyleDefault];
        cell.textLabel.text = @"静音";
        UISwitch *sw = [[UISwitch alloc] init];
        sw.tag = 201;
        sw.on = NO;
        [sw addTarget:self action:@selector(switchChanged:)
             forControlEvents:UIControlEventValueChanged];
        cell.accessoryView = sw;
    } else if (indexPath.section == 3 && indexPath.row == 0) {
        cell = [self cellWithIdentifier:@"Version" style:UITableViewCellStyleSubtitle];
        cell.textLabel.text = @"内核版本";
        cell.detailTextLabel.text = [[J2MEBridge shared] kernelVersion];
        cell.detailTextLabel.numberOfLines = 3;
    } else if (indexPath.section == 3 && indexPath.row == 1) {
        cell = [self cellWithIdentifier:@"Diag" style:UITableViewCellStyleDefault];
        cell.textLabel.text = @"查看运行诊断";
        cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
        cell.selectionStyle = UITableViewCellSelectionStyleDefault;
    } else {
        cell = [self cellWithIdentifier:@"Log" style:UITableViewCellStyleDefault];
        cell.textLabel.text = @"查看日志";
        cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
        cell.selectionStyle = UITableViewCellSelectionStyleDefault;
    }
    return cell;
}

- (void)addLabel:(NSString *)text toCell:(UITableViewCell *)cell {
    /* accessoryView 占位时，用 contentView 上的 label 显示标题 */
    for (UIView *v in cell.contentView.subviews)
        if (v.tag == 999) { [v removeFromSuperview]; }
    UILabel *label = [[UILabel alloc] initWithFrame:CGRectMake(16, 8, 110, 28)];
    label.tag = 999;
    label.text = text;
    label.font = [UIFont systemFontOfSize:15];
    label.textColor = [UIColor whiteColor];
    [cell.contentView addSubview:label];
}

- (void)segmentChanged:(UISegmentedControl *)sender {
    if (sender.tag == 100)
        [J2MESettings setScaleMode:(J2MEScaleMode)sender.selectedSegmentIndex];
    else
        [J2MESettings setFilterMode:(J2MEFilterMode)sender.selectedSegmentIndex];
}

- (void)switchChanged:(UISwitch *)sender {
    if (sender.tag == 200) {
        [J2MESettings setKeypadVisibleByDefault:sender.on];
        [[J2MEBridge shared] setKeypadVisible:sender.on];
    } else {
        [[J2MEBridge shared] setMuted:sender.on];
    }
}

- (void)sliderChanged:(UISlider *)sender {
    int value = (int)lround(sender.value);
    [J2MESettings setMasterVolume:value];
    [[J2MEBridge shared] setVolume:value];
    [self.tableView reloadRowsAtIndexPaths:
        @[[NSIndexPath indexPathForRow:0 inSection:2]]
                          withRowAnimation:UITableViewRowAnimationNone];
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    if (indexPath.section != 3 || indexPath.row == 0)
        return;
    J2METextViewController *textVC = [[J2METextViewController alloc] init];
    if (indexPath.row == 1) {
        textVC.title = @"运行诊断";
        textVC.text = [[J2MEBridge shared] diagnosticsText];
    } else {
        textVC.title = @"日志";
        textVC.text = [[[J2MEBridge shared] logLines] componentsJoinedByString:@"\n"];
    }
    [self.navigationController pushViewController:textVC animated:YES];
}

@end
