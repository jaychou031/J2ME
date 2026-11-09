/*
 * 240×320 RGB565 帧缓冲 —— 替换原项目的「固件物理 framebuffer」
 *
 * 原项目：Java screen buffer 直接写到动态取得的固件 framebuffer（153600 字节）。
 * iOS：维护一块同尺寸同像素格式的内存 surface，完整保留原有的所有像素级
 * 合成规则（悬浮点、半透明软键盘、脏矩形刷新语义），只是把「提交」这一步
 * 变成回调给 UIKit 层做缩放显示。
 */
#include "ios_platform.h"

#include <string.h>

static uint16_t  g_framebuffer[IOS_FB_W * IOS_FB_H];
static IOSFrameSink g_sink = NULL;
static void      *g_sink_user = NULL;
static uint32_t   g_present_count = 0u;

int ios_fb_init(void) {
    ios_fb_clear(IOS_RGB565_BLACK);
    g_present_count = 0u;
    return 0;
}

void ios_fb_shutdown(void) {
    g_sink = NULL;
    g_sink_user = NULL;
}

uint16_t *ios_fb_buffer(void) {
    return g_framebuffer;
}

void ios_fb_set_sink(IOSFrameSink sink, void *user) {
    g_sink = sink;
    g_sink_user = user;
}

void ios_fb_clear(uint16_t color) {
    int i;
    for (i = 0; i < IOS_FB_W * IOS_FB_H; ++i)
        g_framebuffer[i] = color;
}

static void clip_rect(int *x, int *y, int *w, int *h) {
    int x1 = *x, y1 = *y, x2 = *x + *w, y2 = *y + *h;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > IOS_FB_W) x2 = IOS_FB_W;
    if (y2 > IOS_FB_H) y2 = IOS_FB_H;
    *x = x1;
    *y = y1;
    *w = x2 - x1;
    *h = y2 - y1;
}

void ios_fb_fill_rect(int x, int y, int w, int h, uint16_t color) {
    int cx = x, cy = y, cw = w, ch = h;
    int row;
    clip_rect(&cx, &cy, &cw, &ch);
    if (cw <= 0 || ch <= 0)
        return;
    for (row = 0; row < ch; ++row) {
        int col;
        uint16_t *line = g_framebuffer + (size_t)(cy + row) * IOS_FB_W + (size_t)cx;
        for (col = 0; col < cw; ++col)
            line[col] = color;
    }
}

/* 原项目 softpad_blend()：alpha 以 /256 表示，底板 224/256、普通键 208/256 */
static uint16_t blend_pixel(uint16_t base, uint16_t overlay, uint32_t alpha256) {
    uint32_t r, g, b;
    uint32_t br = (base >> 11) & 0x1fu;
    uint32_t bg = (base >> 5)  & 0x3fu;
    uint32_t bb = base & 0x1fu;
    uint32_t orr = (overlay >> 11) & 0x1fu;
    uint32_t og = (overlay >> 5)  & 0x3fu;
    uint32_t ob = overlay & 0x1fu;
    uint32_t inv = 256u - alpha256;
    if (alpha256 >= 256u)
        return overlay;
    r = (br * inv + orr * alpha256) >> 8;
    g = (bg * inv + og * alpha256) >> 8;
    b = (bb * inv + ob * alpha256) >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void ios_fb_blend_rect(int x, int y, int w, int h,
                       uint16_t color, uint32_t alpha256) {
    int cx = x, cy = y, cw = w, ch = h;
    int row, col;
    clip_rect(&cx, &cy, &cw, &ch);
    if (cw <= 0 || ch <= 0 || alpha256 == 0u)
        return;
    for (row = 0; row < ch; ++row) {
        uint16_t *line = g_framebuffer + (size_t)(cy + row) * IOS_FB_W;
        for (col = 0; col < cw; ++col) {
            int px = cx + col;
            line[px] = blend_pixel(line[px], color, alpha256);
        }
    }
}

void ios_fb_hline(int x, int y, int w, uint16_t color) {
    ios_fb_fill_rect(x, y, w, 1, color);
}

void ios_fb_vline(int x, int y, int h, uint16_t color) {
    ios_fb_fill_rect(x, y, 1, h, color);
}

void ios_fb_outline_rect(int x, int y, int w, int h,
                         uint16_t color, int thickness) {
    int t = thickness < 1 ? 1 : thickness;
    ios_fb_hline(x, y, w, color);
    ios_fb_hline(x, y + h - 1, w, color);
    ios_fb_vline(x, y, h, color);
    ios_fb_vline(x + w - 1, y, h, color);
    if (t > 1) {
        ios_fb_hline(x, y + 1, w, color);
        ios_fb_hline(x, y + h - 2, w, color);
        ios_fb_vline(x + 1, y, h, color);
        ios_fb_vline(x + w - 2, y, h, color);
    }
}

void ios_fb_present(void) {
    /* 覆盖层在最终帧上单遍合成，与原项目「深色单遍合成版」一致：
     * 游戏持续更新背后的 Java surface，浮层只负责呈现，不占额外全屏缓冲。 */
    ios_softpad_composite(g_framebuffer, IOS_FB_W, IOS_FB_H);
    ++g_present_count;
    if (g_sink != NULL)
        g_sink(g_framebuffer, IOS_FB_W, IOS_FB_H, g_sink_user);
}

uint32_t ios_fb_present_count(void) {
    return g_present_count;
}
