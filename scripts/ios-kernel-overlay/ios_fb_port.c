/*
 * MIDP fb_port 的 iOS 实现
 *
 * 对应原项目 phoneme_bbk9588_port.c 末尾的 fb_port 段：
 *   gxj_system_screen_buffer（Java screen buffer）
 *     -> refreshScreenNormal/Rotated
 *     -> ios_fb_buffer()（240×320 RGB565 内存 surface）
 *     -> ios_fb_present() 合成软键盘后回调 App 缩放显示
 *
 * 与原项目差别：不再做面板区域的跳过与逐像素混色（那些优化是为了 9588
 * 的慢速 framebuffer 与 PCM 补写），统一交给 ios_fb_present() 单遍合成。
 */
#include <string.h>

#include "gxj_screen_buffer.h"
#include "gxj_intern_graphics.h"
#include "gxj_putpixel.h"
#include "midpMalloc.h"

#include "ios_platform.h"

extern gxj_screen_buffer gxj_system_screen_buffer;

void connectFrameBuffer(void) {
    ios_fb_init();
}

void initScreenBuffer(int width, int height) {
    (void)gxj_init_screen_buffer(width, height);
}

void clearScreen(void) {
    ios_fb_clear(IOS_RGB565_BLACK);
}

void resizeScreenBuffer(int width, int height) {
    (void)gxj_resize_screen_buffer(width, height);
}

void reverseScreenOrientation(void) {
    (void)gxj_rotate_screen_buffer(0);
}

int getScreenX(int screenRotated) { (void)screenRotated; return 0; }
int getScreenY(int screenRotated) { (void)screenRotated; return 0; }
int getKeyboardFd(void) { return -1; }
int getMouseFd(void) { return -1; }

void finalizeFrameBuffer(void) {
    (void)gxj_free_screen_buffer();
}

static void present_rect(int x1, int y1, int x2, int y2, int rotated) {
    gxj_pixel_type *source = gxj_system_screen_buffer.pixelData;
    uint16_t *destination = ios_fb_buffer();
    int width = gxj_system_screen_buffer.width;
    int height = gxj_system_screen_buffer.height;
    int x, y;
    if (source == NULL || destination == NULL)
        return;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > width) x2 = width;
    if (y2 > height) y2 = height;
    for (y = y1; y < y2; ++y) {
        for (x = x1; x < x2; ++x) {
            int dx = x;
            int dy = y;
            uint16_t pixel = (uint16_t)source[y * width + x];
            if (rotated) {
                dx = height - 1 - y;
                dy = x;
            }
            if (dx >= 0 && dx < IOS_FB_W && dy >= 0 && dy < IOS_FB_H)
                destination[dy * IOS_FB_W + dx] = pixel;
        }
    }
}

void refreshScreenNormal(int x1, int y1, int x2, int y2) {
    present_rect(x1, y1, x2, y2, 0);
}

void refreshScreenRotated(int x1, int y1, int x2, int y2) {
    present_rect(x1, y1, x2, y2, 1);
}
