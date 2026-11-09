/*
 * BBK 9588 J2ME / iOS —— 内核抽象层 (ABI)
 *
 * 这是 App 层（Objective-C / UIKit）与 J2ME 运行时之间唯一的接口。
 * 设计目标是让「原项目 phoneME 内核」与「内置诊断内核」可以互换，
 * 从而在 GitHub Actions 上分阶段验证：先保证 App 一定能编译、能装、能缩放，
 * 再逐步把 phoneME 从源码重编译成 arm64 静态库接进来。
 *
 * 与原项目的对应关系见 docs/PORTING.md。
 */
#ifndef J2ME_KERNEL_H
#define J2ME_KERNEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 0 = 内置诊断内核（默认，保证工程随时可编译/可安装）
 * 1 = 链接 CI 从 phoneME 源码重编译出来的 libphoneme_ios.a
 * 由 Xcode build setting 注入，见 project.yml。 */
#ifndef J2ME_USE_PHONEME_KERNEL
#define J2ME_USE_PHONEME_KERNEL 0
#endif

#ifndef J2ME_SCREEN_WIDTH
#define J2ME_SCREEN_WIDTH  240
#endif
#ifndef J2ME_SCREEN_HEIGHT
#define J2ME_SCREEN_HEIGHT 320
#endif

/* 沿用原项目：Java heap 3 MiB，BDA 分配器上限 6 MiB。
 * iOS 上内存充裕，但保持默认值可以让行为与 9588 真机一致。 */
#define J2ME_DEFAULT_JAVA_HEAP_BYTES  (3u * 1024u * 1024u)
#define J2ME_DEFAULT_ALLOC_LIMIT     (6u * 1024u * 1024u)

typedef enum {
    J2ME_INPUT_POINTER_DOWN   = 1,
    J2ME_INPUT_POINTER_DRAG   = 2,
    J2ME_INPUT_POINTER_UP     = 3,
    J2ME_INPUT_KEY_DOWN       = 4,
    J2ME_INPUT_KEY_UP         = 5,
    J2ME_INPUT_KEY_REPEAT     = 6
} J2MEInputType;

/* MIDP Canvas 键码（GameCanvas getKeyStates / keyPressed 使用）。
 * 软键 L/R 对应原项目悬浮键盘的 -6 / -7。 */
enum {
    J2ME_KEY_UP         = -1,
    J2ME_KEY_DOWN       = -2,
    J2ME_KEY_LEFT       = -3,
    J2ME_KEY_RIGHT      = -4,
    J2ME_KEY_FIRE       = -5,
    J2ME_KEY_SOFT_LEFT  = -6,
    J2ME_KEY_SOFT_RIGHT = -7,
    J2ME_KEY_NUM0       = 48,
    J2ME_KEY_NUM1       = 49,
    J2ME_KEY_NUM2       = 50,
    J2ME_KEY_NUM3       = 51,
    J2ME_KEY_NUM4       = 52,
    J2ME_KEY_NUM5       = 53,
    J2ME_KEY_NUM6       = 54,
    J2ME_KEY_NUM7       = 55,
    J2ME_KEY_NUM8       = 56,
    J2ME_KEY_NUM9       = 57,
    J2ME_KEY_STAR       = 42,
    J2ME_KEY_POUND      = 35,
    J2ME_KEY_BACK       = -11,   /* MIDP 2.0: BACK  */
    J2ME_KEY_END        = -11
};

typedef enum {
    J2ME_STATE_IDLE      = 0,
    J2ME_STATE_STARTING  = 1,
    J2ME_STATE_RUNNING   = 2,
    J2ME_STATE_STOPPED   = 3,
    J2ME_STATE_ERROR     = 4
} J2MEState;

typedef struct J2MEHostCallbacks {
    /* 一帧 240×320 RGB565 已经合成完毕（含软键盘浮层），App 负责取走并缩放显示。
     * rgb565 只在回调期间有效，App 必须复制。 */
    void (*onFrame)(const uint16_t *rgb565, int width, int height, void *user);

    /* 可选：内核请求播放 22050Hz / 16bit / mono PCM（沿用原项目 MMAPI 输出规格）。
     * 不提供时内核自行走 ios_audio 输出到 AudioQueue。 */
    void (*onPcm)(const int16_t *samples, int frames, void *user);

    void (*onLog)(const char *message, void *user);
    void (*onState)(J2MEState state, void *user);
    /* 软键盘 X 键：对应原项目调用 SDK bda_confirm() 的「是/否」退出确认。
     * 由 App 弹 UIAlertController，只有用户选「是」才调用 shutdown()。 */
    void (*onExitRequest)(void *user);
    void *user;
} J2MEHostCallbacks;

typedef struct J2MEConfig {
    const char *homeDir;        /* UTF-8：RMS / appdb / J2MEPHONE.LOG 根目录 */
    const char *resourceDir;    /* UTF-8：bundle 内 unifont-cjk-14x14.bin 等所在目录 */
    const char *documentsDir;   /* UTF-8：用户 JAR 目录（文件共享导入位置） */
    uint32_t    javaHeapBytes;
    uint32_t    allocLimitBytes;
    int         masterVolume;   /* 0..100，沿用原项目 0–100% 主音量 */
} J2MEConfig;

typedef struct J2MEKernel {
    int  (*init)(const J2MEConfig *config, const J2MEHostCallbacks *host);
    /* 启动一个 JAR。内核内部负责：Manifest MIDlet-1 解析、旧 ZIP 规范化、
     * PCSL 路径别名、RMS 目录准备。返回 0 表示已进入运行态。 */
    int  (*startJar)(const char *jarPathUtf8);
    /* 由 CADisplayLink 每帧驱动一次；返回 0 继续，<0 表示 MIDlet 已请求退出。 */
    int  (*pump)(void);
    /* 输入：pointer 时 a=x, b=y（240×320 逻辑坐标）；key 时 a=键码, b=0。 */
    void (*postInput)(J2MEInputType type, int a, int b);
    void (*setVolume)(int percent);
    int  (*volume)(void);
    void (*setMuted)(int muted);
    void (*setKeypadVisible)(int visible);
    int  (*keypadVisible)(void);
    void (*shutdown)(void);
    J2MEState (*state)(void);
    /* 返回静态文本，内容对应原项目 J2MEPHONE.LOG 的计数器 */
    const char *(*diagnostics)(void);
    const char *(*version)(void);
} J2MEKernel;

/* 由链接进来的内核实现提供。默认（J2ME_USE_PHONEME_KERNEL=0）是 Mock 诊断内核。 */
const J2MEKernel *j2me_kernel_get(void);

#ifdef __cplusplus
}
#endif

#endif /* J2ME_KERNEL_H */
