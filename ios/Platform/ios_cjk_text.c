/*
 * 中文点阵与文本绘制
 *
 * 直接复用原项目 assets/runtime/unifont-cjk-14x14.bin（766812 字节、
 * 28400 个 14×14 1-bit 字形），以及 phoneme_cjk_text.c 的二分查找与
 * 可变字符宽度规则：CJK 命中 14px advance，西文 9px advance，行高 14px。
 *
 * 与原项目唯一的区别：字库不再由 objcopy 链进 ELF 的 .rodata 段
 * （iOS 链接器不支持 objcopy -I binary），而是作为 bundle resource 读入
 * 只读内存。字节内容与查找算法完全一致，因此中文显示效果一致。
 */
#include "ios_platform.h"
#include "ios_latin_font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CJK_HEADER_SIZE   12u
#define CJK_RECORD_SIZE   27u
#define CJK_WIDTH         14
#define CJK_HEIGHT        14
#define CJK_GLYPH_BYTES   25u

static unsigned char *g_font = NULL;
static long           g_font_size = 0;
static uint16_t       g_font_count = 0;

static uint16_t read_u16le(const unsigned char *at) {
    return (uint16_t)((uint16_t)at[0] | ((uint16_t)at[1] << 8));
}

static const unsigned char *cjk_glyph(uint16_t codepoint) {
    uint32_t low = 0u;
    uint32_t high;
    const unsigned char *data = g_font;
    if (data == NULL || g_font_size < (long)CJK_HEADER_SIZE)
        return NULL;
    if (data[0] != 'C' || data[1] != 'J' || data[2] != '1' ||
        data[3] != '4' || data[6] != CJK_WIDTH ||
        data[7] != CJK_HEIGHT || data[8] != CJK_GLYPH_BYTES)
        return NULL;
    high = (uint32_t)g_font_count;
    if ((uint32_t)g_font_size < CJK_HEADER_SIZE + high * CJK_RECORD_SIZE)
        return NULL;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2u;
        const unsigned char *record =
            data + CJK_HEADER_SIZE + middle * CJK_RECORD_SIZE;
        if (read_u16le(record) < codepoint)
            low = middle + 1u;
        else
            high = middle;
    }
    if (low < (uint32_t)g_font_count) {
        const unsigned char *record =
            data + CJK_HEADER_SIZE + low * CJK_RECORD_SIZE;
        if (read_u16le(record) == codepoint)
            return record + 2u;
    }
    return NULL;
}

int ios_cjk_load(const char *resourceDirUtf8) {
    char path[1024];
    FILE *file;
    long size;
    unsigned char *buffer;
    size_t got;
    if (g_font != NULL)
        return 0;
    if (resourceDirUtf8 == NULL)
        return -1;
    if (snprintf(path, sizeof(path), "%s/unifont-cjk-14x14.bin",
                 resourceDirUtf8) >= (int)sizeof(path))
        return -1;
    file = fopen(path, "rb");
    if (file == NULL)
        return -1;
    if (fseek(file, 0L, SEEK_END) != 0) { fclose(file); return -1; }
    size = ftell(file);
    if (size < (long)CJK_HEADER_SIZE || size > 4L * 1024L * 1024L) {
        fclose(file);
        return -1;
    }
    rewind(file);
    buffer = (unsigned char *)malloc((size_t)size);
    if (buffer == NULL) { fclose(file); return -1; }
    got = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (got != (size_t)size) { free(buffer); return -1; }
    if (buffer[0] != 'C' || buffer[1] != 'J' ||
        buffer[2] != '1' || buffer[3] != '4') {
        free(buffer);
        return -1;
    }
    g_font = buffer;
    g_font_size = size;
    g_font_count = read_u16le(buffer + 4u);
    return 0;
}

void ios_cjk_unload(void) {
    free(g_font);
    g_font = NULL;
    g_font_size = 0;
    g_font_count = 0;
}

int ios_cjk_count(void) {
    return (int)g_font_count;
}

int ios_cjk_has(uint16_t codepoint) {
    return cjk_glyph(codepoint) != NULL;
}

int ios_cjk_width(uint16_t codepoint) {
    return cjk_glyph(codepoint) != NULL ? CJK_WIDTH : IOS_LATIN_ADVANCE;
}

void ios_cjk_draw(uint16_t *fb, int fbw, int fbh,
                  int x, int y, uint16_t codepoint, uint16_t color) {
    const unsigned char *glyph = cjk_glyph(codepoint);
    int gx, gy;
    if (glyph == NULL || fb == NULL)
        return;
    for (gy = 0; gy < CJK_HEIGHT; ++gy) {
        int dy = y + gy;
        if (dy < 0 || dy >= fbh)
            continue;
        for (gx = 0; gx < CJK_WIDTH; ++gx) {
            int dx = x + gx;
            unsigned int bit;
            if (dx < 0 || dx >= fbw)
                continue;
            bit = (unsigned int)(gy * CJK_WIDTH + gx);
            if (glyph[bit >> 3] & (unsigned char)(0x80u >> (bit & 7u)))
                fb[(size_t)dy * (size_t)fbw + (size_t)dx] = color;
        }
    }
}

/* ---------- UTF-8 ---------- */

/* 返回本次消耗的字节数（1..4），非法序列按 1 字节吞掉并产出 U+FFFD */
static int utf8_next(const char *s, uint16_t *out) {
    const unsigned char *p = (const unsigned char *)s;
    if (p[0] < 0x80u) { *out = p[0]; return 1; }
    if ((p[0] & 0xe0u) == 0xc0u && (p[1] & 0xc0u) == 0x80u) {
        uint32_t v = ((uint32_t)(p[0] & 0x1fu) << 6) |
                     (uint32_t)(p[1] & 0x3fu);
        *out = (v <= 0xffffu) ? (uint16_t)v : 0xfffdu;
        return 2;
    }
    if ((p[0] & 0xf0u) == 0xe0u && (p[1] & 0xc0u) == 0x80u &&
        (p[2] & 0xc0u) == 0x80u) {
        uint32_t v = ((uint32_t)(p[0] & 0x0fu) << 12) |
                     ((uint32_t)(p[1] & 0x3fu) << 6) |
                     (uint32_t)(p[2] & 0x3fu);
        /* BMP 之外（如部分 CJK 扩展 B）本字库不含，回退为替代符 */
        *out = (v <= 0xffffu) ? (uint16_t)v : 0xfffdu;
        return 3;
    }
    if ((p[0] & 0xf8u) == 0xf0u && (p[1] & 0xc0u) == 0x80u &&
        (p[2] & 0xc0u) == 0x80u && (p[3] & 0xc0u) == 0x80u) {
        *out = 0xfffdu;   /* 4 字节：字库只有 BMP */
        return 4;
    }
    *out = 0xfffdu;
    return 1;
}

int ios_text_init(const char *resourceDirUtf8) {
    return ios_cjk_load(resourceDirUtf8);
}

void ios_text_shutdown(void) {
    ios_cjk_unload();
}

int ios_text_height(void) {
    return IOS_LATIN_LINE_H;
}

int ios_text_width(const char *utf8) {
    int width = 0;
    if (utf8 == NULL)
        return 0;
    while (*utf8 != '\0') {
        uint16_t cp;
        utf8 += utf8_next(utf8, &cp);
        width += ios_cjk_width(cp);
    }
    return width;
}

static void draw_latin(uint16_t *fb, int fbw, int fbh, int x, int y,
                       uint16_t codepoint, uint16_t color) {
    const uint8_t *glyph;
    int gx, gy;
    uint16_t cp = codepoint;
    if (cp < IOS_LATIN_FIRST || cp > IOS_LATIN_LAST || cp == 0xfffdu)
        cp = '?';
    glyph = kIOSLatinGlyphs[cp - IOS_LATIN_FIRST];
    for (gy = 0; gy < IOS_LATIN_GLYPH_H; ++gy) {
        int dy = y + 2 + gy;   /* 下移 2px，与 14×14 汉字基线对齐 */
        if (dy < 0 || dy >= fbh)
            continue;
        for (gx = 0; gx < IOS_LATIN_GLYPH_W; ++gx) {
            int dx = x + gx;
            if (dx < 0 || dx >= fbw)
                continue;
            if (glyph[gy] & (uint8_t)(0x80u >> gx))
                fb[(size_t)dy * (size_t)fbw + (size_t)dx] = color;
        }
    }
}

void ios_text_draw_to(uint16_t *fb, int fbw, int fbh,
                      const char *utf8, int x, int y, uint16_t color) {
    int cursor = x;
    if (fb == NULL || utf8 == NULL)
        return;
    while (*utf8 != '\0') {
        uint16_t cp;
        int width;
        utf8 += utf8_next(utf8, &cp);
        if (ios_cjk_has(cp)) {
            ios_cjk_draw(fb, fbw, fbh, cursor, y, cp, color);
            width = CJK_WIDTH;
        } else {
            draw_latin(fb, fbw, fbh, cursor, y, cp, color);
            width = IOS_LATIN_ADVANCE;
        }
        cursor += width;
        if (cursor >= fbw)
            break;
    }
}

void ios_text_draw(const char *utf8, int x, int y, uint16_t color) {
    uint16_t *fb = ios_fb_buffer();
    if (fb == NULL)
        return;
    ios_text_draw_to(fb, IOS_FB_W, IOS_FB_H, utf8, x, y, color);
}

void ios_text_draw_clipped(const char *utf8, int x, int y,
                           int maxWidth, uint16_t color) {
    int cursor = x;
    char one[5];
    if (utf8 == NULL || maxWidth <= 0)
        return;
    while (*utf8 != '\0') {
        uint16_t cp;
        int n = utf8_next(utf8, &cp);
        int width = ios_cjk_width(cp);
        if (cursor + width > x + maxWidth || cursor + width > IOS_FB_W)
            break;
        if (n <= 0 || n > 4)
            break;
        memcpy(one, utf8, (size_t)n);
        one[n] = '\0';
        ios_text_draw(one, cursor, y, color);
        cursor += width;
        utf8 += n;
    }
}

void ios_text_draw_centered(const char *utf8, int cx, int y, uint16_t color) {
    int x = cx - ios_text_width(utf8) / 2;
    ios_text_draw(utf8, x, y, color);
}
