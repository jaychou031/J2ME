#import "J2MESettings.h"
#import <Foundation/Foundation.h>

NSString *const J2MESettingsDidChangeNotification = @"J2MESettingsDidChangeNotification";

static NSString *const kScaleMode = @"J2MEScaleMode";
static NSString *const kFilterMode = @"J2MEFilterMode";
static NSString *const kKeypad = @"J2MEKeypadVisibleByDefault";
static NSString *const kVolume = @"J2MEMasterVolume";

@implementation J2MESettings

+ (void)notify {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:J2MESettingsDidChangeNotification object:nil];
}

+ (J2MEScaleMode)scaleMode {
    NSInteger v = [[NSUserDefaults standardUserDefaults] integerForKey:kScaleMode];
    return (J2MEScaleMode)v;
}
+ (void)setScaleMode:(J2MEScaleMode)mode {
    [[NSUserDefaults standardUserDefaults] setInteger:mode forKey:kScaleMode];
    [self notify];
}

+ (J2MEFilterMode)filterMode {
    NSInteger v = [[NSUserDefaults standardUserDefaults] integerForKey:kFilterMode];
    return (J2MEFilterMode)v;
}
+ (void)setFilterMode:(J2MEFilterMode)mode {
    [[NSUserDefaults standardUserDefaults] setInteger:mode forKey:kFilterMode];
    [self notify];
}

+ (BOOL)keypadVisibleByDefault {
    /* 默认展开软键盘，和 9588 真机一致 */
    NSNumber *v = [[NSUserDefaults standardUserDefaults] objectForKey:kKeypad];
    return v ? [v boolValue] : YES;
}
+ (void)setKeypadVisibleByDefault:(BOOL)visible {
    [[NSUserDefaults standardUserDefaults] setBool:visible forKey:kKeypad];
    [self notify];
}

+ (int)masterVolume {
    NSNumber *v = [[NSUserDefaults standardUserDefaults] objectForKey:kVolume];
    return v ? [v intValue] : 100;
}
+ (void)setMasterVolume:(int)percent {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    [[NSUserDefaults standardUserDefaults] setInteger:percent forKey:kVolume];
    [self notify];
}

@end
