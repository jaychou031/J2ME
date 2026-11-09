#import "J2MEScreenView.h"
#import "J2MEBridge.h"

#include "j2me_kernel.h"
#include <string.h>

#import <QuartzCore/CoreAnimation.h>

@interface J2MEScreenView () {
    CALayer          *_screenLayer;
    CGContextRef      _bitmapContext;
    CGColorSpaceRef   _colorSpace;
    NSInteger         _logicalWidth;
    NSInteger         _logicalHeight;
    CGFloat           _scaleFactor;
    CGRect            _screenRect;
}
@end

@implementation J2MEScreenView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self commonInit];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    self = [super initWithCoder:coder];
    if (self) {
        [self commonInit];
    }
    return self;
}

- (void)commonInit {
    _logicalWidth = J2ME_SCREEN_WIDTH;
    _logicalHeight = J2ME_SCREEN_HEIGHT;
    _scaleFactor = 1.0;
    _screenRect = self.bounds;
    _scaleMode = J2MEScaleModePixelPerfect;
    _filterMode = J2MEFilterModeNearest;

    self.backgroundColor = [UIColor colorWithRed:0.06 green:0.07 blue:0.09 alpha:1.0];
    self.multipleTouchEnabled = NO;
    self.clipsToBounds = YES;

    _colorSpace = CGColorSpaceCreateDeviceRGB();
    _bitmapContext = CGBitmapContextCreate(
        NULL, (size_t)_logicalWidth, (size_t)_logicalHeight, 8,
        (size_t)_logicalWidth * 4, _colorSpace,
        (CGBitmapInfo)(kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Little));

    _screenLayer = [CALayer layer];
    _screenLayer.contentsScale = [UIScreen mainScreen].scale;
    _screenLayer.contentsGravity = kCAGravityResize;
    _screenLayer.magnificationFilter = kCAFilterNearest;
    _screenLayer.minificationFilter = kCAFilterNearest;
    _screenLayer.backgroundColor = [UIColor blackColor].CGColor;
    [self.layer addSublayer:_screenLayer];

    /* 首次给一帧纯黑，避免启动时空 layer 闪烁 */
    [self clearToBlack];
}

- (void)dealloc {
    if (_bitmapContext) { CGContextRelease(_bitmapContext); _bitmapContext = NULL; }
    if (_colorSpace)    { CGColorSpaceRelease(_colorSpace);  _colorSpace = NULL; }
}

- (void)clearToBlack {
    if (_bitmapContext == NULL)
        return;
    size_t bytes = CGBitmapContextGetHeight(_bitmapContext) *
                   CGBitmapContextGetBytesPerRow(_bitmapContext);
    memset(CGBitmapContextGetData(_bitmapContext), 0, bytes);
    CGImageRef image = CGBitmapContextCreateImage(_bitmapContext);
    if (image) {
        _screenLayer.contents = (__bridge id)image;
        CGImageRelease(image);
    }
}

- (void)setScaleMode:(J2MEScaleMode)scaleMode {
    _scaleMode = scaleMode;
    [self setNeedsLayout];
}

- (void)setFilterMode:(J2MEFilterMode)filterMode {
    _filterMode = filterMode;
    NSString *filter = (filterMode == J2MEFilterModeNearest)
                       ? kCAFilterNearest : kCAFilterLinear;
    _screenLayer.magnificationFilter = filter;
    _screenLayer.minificationFilter = filter;
}

- (CGFloat)currentScale { return _scaleFactor; }
- (CGRect)screenRect { return _screenRect; }

- (void)layoutSubviews {
    [super layoutSubviews];
    CGRect bounds = self.bounds;
    CGFloat logicalW = (CGFloat)_logicalWidth;
    CGFloat logicalH = (CGFloat)_logicalHeight;
    CGFloat scale = 1.0;
    CGFloat width, height;

    switch (_scaleMode) {
        case J2MEScaleModePixelPerfect:
            scale = MAX(1.0, floor(MIN(bounds.size.width / logicalW,
                                       bounds.size.height / logicalH)));
            break;
        case J2MEScaleModeAspectFit:
            scale = MIN(bounds.size.width / logicalW,
                        bounds.size.height / logicalH);
            if (scale <= 0) scale = 1.0;
            break;
        case J2MEScaleModeStretch:
            _scaleFactor = MIN(bounds.size.width / logicalW,
                               bounds.size.height / logicalH);
            if (_scaleFactor <= 0) _scaleFactor = 1.0;
            _screenRect = bounds;
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            _screenLayer.frame = bounds;
            [CATransaction commit];
            return;
    }
    width = logicalW * scale;
    height = logicalH * scale;
    _scaleFactor = scale;
    _screenRect = CGRectMake((bounds.size.width - width) / 2.0,
                             (bounds.size.height - height) / 2.0,
                             width, height);
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    _screenLayer.frame = _screenRect;
    [CATransaction commit];
}

- (void)setFrameBuffer:(const uint16_t *)rgb565
                 width:(int)width height:(int)height {
    if (rgb565 == NULL || _bitmapContext == NULL)
        return;
    if (width != _logicalWidth || height != _logicalHeight) {
        /* 内核未来若支持其它分辨率，这里自动重建位图 */
        CGContextRelease(_bitmapContext);
        _logicalWidth = width;
        _logicalHeight = height;
        _bitmapContext = CGBitmapContextCreate(
            NULL, (size_t)width, (size_t)height, 8, (size_t)width * 4,
            _colorSpace,
            (CGBitmapInfo)(kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Little));
        if (_bitmapContext == NULL)
            return;
        [self setNeedsLayout];
    }
    uint8_t *data = (uint8_t *)CGBitmapContextGetData(_bitmapContext);
    NSInteger count = (NSInteger)width * (NSInteger)height;
    NSInteger i;
    for (i = 0; i < count; ++i) {
        uint16_t c = rgb565[i];
        uint8_t r5 = (uint8_t)((c >> 11) & 0x1fu);
        uint8_t g6 = (uint8_t)((c >> 5) & 0x3fu);
        uint8_t b5 = (uint8_t)(c & 0x1fu);
        data[i * 4 + 0] = (uint8_t)((r5 << 3) | (r5 >> 2));
        data[i * 4 + 1] = (uint8_t)((g6 << 2) | (g6 >> 4));
        data[i * 4 + 2] = (uint8_t)((b5 << 3) | (b5 >> 2));
        data[i * 4 + 3] = 255u;
    }
    CGImageRef image = CGBitmapContextCreateImage(_bitmapContext);
    if (image != NULL) {
        /* pump() 由主线程的 CADisplayLink 驱动，直接同步赋值即可 */
        _screenLayer.contents = (__bridge id)image;
        CGImageRelease(image);
    }
}

- (CGPoint)logicalPointForLocation:(CGPoint)location {
    CGRect rect = _screenRect;
    CGFloat x = (location.x - rect.origin.x) / (rect.size.width /
                                                (CGFloat)_logicalWidth);
    CGFloat y = (location.y - rect.origin.y) / (rect.size.height /
                                                (CGFloat)_logicalHeight);
    return CGPointMake(x, y);
}

- (UIImage *)snapshot {
    if (_bitmapContext == NULL)
        return nil;
    CGImageRef image = CGBitmapContextCreateImage(_bitmapContext);
    if (image == NULL)
        return nil;
    UIImage *result = [UIImage imageWithCGImage:image
                                          scale:1.0
                                    orientation:UIImageOrientationUp];
    CGImageRelease(image);
    return result;
}

/* ---------- 触摸 → 240×320 逻辑坐标 ---------- */

static int ClampLogical(CGFloat value, NSInteger maximum) {
    if (value < 0) return 0;
    if (value > maximum - 1) return (int)(maximum - 1);
    return (int)value;
}

- (void)sendPointer:(int)type withTouch:(UITouch *)touch invalid:(BOOL)invalid {
    J2MEBridge *bridge = [J2MEBridge shared];
    if (invalid) {
        /* 真机 UP 坐标可能失效：沿用最后一次有效坐标的语义由 C 层处理 */
        [bridge postPointerType:type x:-1 y:-1];
        return;
    }
    CGPoint p = [self logicalPointForLocation:[touch locationInView:self]];
    [bridge postPointerType:type
                          x:ClampLogical(p.x, _logicalWidth)
                          y:ClampLogical(p.y, _logicalHeight)];
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    UITouch *touch = [touches anyObject];
    if (touch) [self sendPointer:J2ME_INPUT_POINTER_DOWN withTouch:touch invalid:NO];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    UITouch *touch = [touches anyObject];
    if (touch) [self sendPointer:J2ME_INPUT_POINTER_DRAG withTouch:touch invalid:NO];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    UITouch *touch = [touches anyObject];
    if (!touch) return;
    CGPoint p = [self logicalPointForLocation:[touch locationInView:self]];
    BOOL outside = (p.x < 0 || p.y < 0 ||
                    p.x >= _logicalWidth || p.y >= _logicalHeight);
    [self sendPointer:J2ME_INPUT_POINTER_UP withTouch:touch invalid:outside];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    (void)event;
    UITouch *touch = [touches anyObject];
    if (touch) [self sendPointer:J2ME_INPUT_POINTER_UP withTouch:touch invalid:NO];
}

@end
