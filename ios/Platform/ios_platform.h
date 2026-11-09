/*
 * BBK 9588 J2ME / iOS —— 平台适配层总头文件
 *
 * 这一层替换原项目 src/platform/bbk9588/ 里所有与 BBK 9588 固件耦合的部分：
 *   bda_gui_* / bda_audio / framebuffer / GBK 路径 / freestanding libc
 * 换成：
 *   内存 RGB565 surface + UIKit 缩放 / AudioQueue / POSIX 文件 / libSystem
 *
 * 保持原样的部分：
 *   - 240×320 RGB565 逻辑分辨率与像素格式
 *   - 事件队列（32 深、move 合并、关键事件不丢弃）—— native_events.c 的语义
 *   - 14×14 CJK 点阵查找与绘制 —— phoneme_cjk_text.c 的算法与字库文件
 *   - 老式 ZIP data descriptor 规范化 —— phoneme_pcsl_file.c 的算法
 *   - 悬浮点 + 半透明软键盘 + 音量条 —— phoneme_bbk9588_port.c 的布局与合成规则
 *   - 22050Hz / 16bit / mono PCM、1024-byte 块、0–100% 主音量 —— phoneme_audio.c
 */
#ifndef IOS_PLATFORM_H
#define IOS_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#include "j2me_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- RGB565 ---------- */
#define IOS_RGB565(r, g, b) \
    ((uint16_t)((((uint16_t)(r) >> 3) << 11) | \
                (((uint16_t)(g) >> 2) << 5)  | \
                ((uint16_t)(b) >> 3)))
#define IOS_RGB565_WHITE  IOS_RGB565(255,255,255)
#define IOS_RGB565_BLACK  IOS_RGB565(0,0,0)

/* ---------- 时间 ---------- */
/* 对应原项目 phoneme_time_ms()：单调毫秒时钟，iOS 上用 mach_absolute_time /
 * clock_gettime(CLOCK_MONOTONIC)，不会因为系统时间被修改而倒退。 */
uint32_t ios_j2me_now_ms(void);
void     ios_j2me_sleep_ms(uint32_t ms);

/* ---------- 帧缓冲 ---------- */
#define IOS_FB_W J2ME_SCREEN_WIDTH
#define IOS_FB_H J2ME_SCREEN_HEIGHT

typedef void (*IOSFrameSink)(const uint16_t *rgb565, int width, int height, void *user);

int       ios_fb_init(void);
void      ios_fb_shutdown(void);
uint16_t *ios_fb_buffer(void);                 /* 长度 IOS_FB_W*IOS_FB_H */
void      ios_fb_set_sink(IOSFrameSink sink, void *user);
void      ios_fb_clear(uint16_t color);
void      ios_fb_fill_rect(int x, int y, int w, int h, uint16_t color);
void      ios_fb_blend_rect(int x, int y, int w, int h, uint16_t color, uint32_t alpha256);
void      ios_fb_outline_rect(int x, int y, int w, int h, uint16_t color, int thickness);
void      ios_fb_hline(int x, int y, int w, uint16_t color);
void      ios_fb_vline(int x, int y, int h, uint16_t color);
/* 合成软键盘浮层并把最终帧交给 sink（对应原项目 present 路径） */
void      ios_fb_present(void);
uint32_t  ios_fb_present_count(void);

/* ---------- 文本（原项目：西文 9×14 advance，CJK 14×14 / 14px advance） ---------- */
int  ios_text_init(const char *resourceDirUtf8);
void ios_text_shutdown(void);
void ios_text_draw(const char *utf8, int x, int y, uint16_t color);
void ios_text_draw_clipped(const char *utf8, int x, int y, int maxWidth, uint16_t color);
void ios_text_draw_centered(const char *utf8, int cx, int y, uint16_t color);
int  ios_text_width(const char *utf8);
int  ios_text_height(void);
/* 直接把 UTF-8 字符串画进任意 RGB565 buffer（Mock/内核 overlay 用） */
void ios_text_draw_to(uint16_t *fb, int fbw, int fbh,
                      const char *utf8, int x, int y, uint16_t color);

/* ---------- CJK 字库（原项目 assets/runtime/unifont-cjk-14x14.bin） ---------- */
/* 文件格式：
 *   [0..3]   "CJ14"
 *   [4..5]   字形数 u16le（28400）
 *   [6]      宽度 14   [7] 高度 14   [8] 每字形字节数 25   [9..11] 保留
 *   [12..]   记录：codepoint u16le + 25 字节 1-bit 位图（MSB first，行优先）
 *   记录按 codepoint 升序，二分查找。
 */
int  ios_cjk_load(const char *resourceDirUtf8);
void ios_cjk_unload(void);
int  ios_cjk_count(void);
int  ios_cjk_has(uint16_t codepoint);
int  ios_cjk_width(uint16_t codepoint);      /* 命中返回 14，否则返回西文宽度 */
void ios_cjk_draw(uint16_t *fb, int fbw, int fbh,
                  int x, int y, uint16_t codepoint, uint16_t color);

/* ---------- 事件队列（原样移植 native_events.c 的语义） ---------- */
#define IOS_EVENT_CAPACITY 32u

typedef struct {
    J2MEInputType type;
    int16_t x, y;
    int32_t key;
    uint32_t timestamp_ms;
} IOSNativeEvent;

typedef struct {
    volatile uint32_t read_index;
    volatile uint32_t write_index;
    volatile uint32_t dropped;
    volatile uint32_t dropped_critical;
    volatile uint32_t move_coalesced;
    volatile uint32_t peak_depth;
    IOSNativeEvent events[IOS_EVENT_CAPACITY];
} IOSNativeEventQueue;

void ios_event_queue_reset(IOSNativeEventQueue *q);
void ios_event_push(IOSNativeEventQueue *q, J2MEInputType type, int x, int y, uint32_t ms);
void ios_event_push_key(IOSNativeEventQueue *q, J2MEInputType type, int key, uint32_t ms);
int  ios_event_pop(IOSNativeEventQueue *q, IOSNativeEvent *out);
extern IOSNativeEventQueue g_j2me_events;

/* ---------- JAR / Manifest / 老式 ZIP 规范化 ---------- */
typedef struct {
    unsigned char *data;
    long size;
    int  normalized;      /* 1 = 做过 data descriptor 规范化 */
    char midlet1[256];    /* META-INF/MANIFEST.MF 的 MIDlet-1 主类名 */
    char midletName[128]; /* MIDlet-1 的显示名（逗号前第一段） */
} IOSJar;

/* 读取 JAR 到内存（上限沿用原项目 2 MiB 缓存策略，可由 maxBytes 放宽）、
 * 自动做老式 ZIP 规范化，并解析 MIDlet-1。返回 0 成功。 */
int  ios_jar_open(const char *pathUtf8, IOSJar *out, long maxBytes);
void ios_jar_close(IOSJar *jar);
/* 只做解析，不持有文件（供 UIDocumentPicker 预览 Manifest 用） */
int  ios_jar_parse_midlet1(const unsigned char *data, long size,
                           char *outClass, size_t classCap,
                           char *outName, size_t nameCap);
/* 把 JAR 内的 MANIFEST.MF 原始文本复制出来（调用者 free） */
char *ios_jar_read_manifest(const unsigned char *data, long size, long *outLen);
/* 老式 ZIP 规范化：只在内存缓存里补齐 local header、移除 descriptor、修正
 * central directory offset；不改动任何 class / resource 字节。 */
int  ios_jar_normalize(unsigned char **dataPtr, long *sizePtr);

/* ---------- 软键盘 / 悬浮点 / 音量条（复刻原项目布局） ---------- */
#define IOS_SOFTPAD_FLOAT_DIAMETER 31
#define IOS_SOFTPAD_FLOAT_HIT       44
#define IOS_SOFTPAD_DRAG_THRESHOLD  12
#define IOS_SOFTPAD_PANEL_ALPHA     224  /* /256 黑色底板 */
#define IOS_SOFTPAD_KEY_ALPHA       208  /* /256 普通按键 */

typedef enum {
    IOS_SOFTPAD_ACTION_NONE = 0,
    IOS_SOFTPAD_ACTION_EXIT,      /* X：对应原项目 SDK bda_confirm 退出确认 */
    IOS_SOFTPAD_ACTION_VOL_DOWN,  /* -  每次 10% */
    IOS_SOFTPAD_ACTION_VOL_UP,    /* +  每次 10% */
    IOS_SOFTPAD_ACTION_MUTE       /* M */
} IOSSoftpadAction;

typedef void (*IOSSoftpadActionHandler)(IOSSoftpadAction action, void *user);

void ios_softpad_init(void);
void ios_softpad_shutdown(void);
void ios_softpad_set_action_handler(IOSSoftpadActionHandler handler, void *user);
void ios_softpad_set_visible(int visible);
int  ios_softpad_visible(void);
void ios_softpad_set_volume(int percent, int muted);
/* 输入坐标是 240×320 逻辑坐标。返回 1 表示该触摸被浮层消费（不传给游戏）。 */
int  ios_softpad_handle_touch(J2MEInputType type, int x, int y, uint32_t ms);
/* 合成到当前帧缓冲（在 ios_fb_present() 内调用） */
void ios_softpad_composite(uint16_t *fb, int fbw, int fbh);
/* 诊断计数（对应原项目 J2MEPHONE.LOG） */
uint32_t ios_softpad_toggle_count(void);
uint32_t ios_softpad_key_event_count(void);
uint32_t ios_softpad_drag_count(void);
uint32_t ios_softpad_exit_count(void);

/* ---------- 音频（原项目 MMAPI 输出规格） ---------- */
#define IOS_AUDIO_SAMPLE_RATE 22050
#define IOS_AUDIO_CHANNELS    1
#define IOS_AUDIO_BLOCK_BYTES 1024

int  ios_audio_init(void);
void ios_audio_shutdown(void);
/* 非阻塞写入 16-bit mono PCM；内部做 0–100% 数字主音量。
 * 返回实际写入的帧数。 */
int  ios_audio_write(const int16_t *samples, int frames);
int  ios_audio_ready(void);
void ios_audio_stop(void);
void ios_audio_set_volume(int percent);
int  ios_audio_volume(void);
void ios_audio_set_muted(int muted);
int  ios_audio_muted(void);
void ios_audio_pump(void);
uint32_t ios_audio_write_count(void);
uint32_t ios_audio_write_bytes(void);
uint32_t ios_audio_ready_misses(void);

#ifdef __cplusplus
}
#endif

#endif /* IOS_PLATFORM_H */
