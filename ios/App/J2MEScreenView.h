#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, J2MEScaleMode) {
    /* 整数倍最近邻：iPhone 13 竖屏可用 4× (960×1280 px)，像素边缘最锐利，
     * 最接近 9588 真机 240×320 物理屏的观感。默认。 */
    J2MEScaleModePixelPerfect = 0,
    /* 等比适配可用区域（允许非整数倍） */
    J2MEScaleModeAspectFit,
    /* 拉伸填满（会改变 3:4 宽高比，一般不推荐） */
    J2MEScaleModeStretch
};

typedef NS_ENUM(NSInteger, J2MEFilterMode) {
    J2MEFilterModeNearest = 0,   /* 像素完美 */
    J2MEFilterModeLinear         /* 双线性平滑 */
};

/* 240×320 RGB565 → CGImage → CALayer，负责缩放显示与触摸坐标反变换。
 * 逻辑坐标始终是 240×320，因此 C 层的软键盘命中测试完全不用改。 */
@interface J2MEScreenView : UIView

@property (nonatomic) J2MEScaleMode scaleMode;
@property (nonatomic) J2MEFilterMode filterMode;
@property (nonatomic, readonly) CGFloat currentScale;
@property (nonatomic, readonly) CGRect screenRect;

- (void)setFrameBuffer:(const uint16_t *)rgb565 width:(int)width height:(int)height;
- (CGPoint)logicalPointForLocation:(CGPoint)location;
- (UIImage *)snapshot;

@end

NS_ASSUME_NONNULL_END
