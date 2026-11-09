#import <Foundation/Foundation.h>
#import "J2MEScreenView.h"

NS_ASSUME_NONNULL_BEGIN

extern NSString *const J2MESettingsDidChangeNotification;

@interface J2MESettings : NSObject
+ (J2MEScaleMode)scaleMode;
+ (void)setScaleMode:(J2MEScaleMode)mode;
+ (J2MEFilterMode)filterMode;
+ (void)setFilterMode:(J2MEFilterMode)mode;
+ (BOOL)keypadVisibleByDefault;
+ (void)setKeypadVisibleByDefault:(BOOL)visible;
+ (int)masterVolume;
+ (void)setMasterVolume:(int)percent;
@end

NS_ASSUME_NONNULL_END
