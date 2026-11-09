/*
 * phoneME / iOS 桥接实现（编译进 libphoneme_ios.a）
 *
 * 这一层把原项目 src/platform/bbk9588/ 的固件依赖，换成 iOS 平台层：
 *   framebuffer   -> ios_fb_buffer() + onFrame 回调
 *   原始输入      -> g_j2me_events 队列
 *   PCSL 文件     -> POSIX（见 ios_pcsl_compat.c）
 *   bda_audio     -> ios_audio_write() -> AudioQueue
 *   主音量/软键盘 -> ios_softpad_* / ios_audio_set_volume()
 *
 * 编译方式见 scripts/build-phoneme-ios.sh。
 *
 * ⚠️ 验证边界（务必阅读 docs/KERNEL_STATUS.md）：
 *   start_jar()/pump() 中与 KVM master mode 对接的两处调用，依赖
 *   phoneME Feature MR2 在特定工具链下重新 romize 后暴露的确切符号。
 *   本仓库的 Mock 内核和整个 UI 层已在本地用 gcc + 真实字库端到端验证；
 *   phoneME 全量源码在 arm64-apple-ios 下的编译，需要在 GitHub Actions
 *   的 macOS runner 上按 BUILD_FAILURE.md 的日志迭代一次。
 *   脚本对此设计了自动降级：失败时仍产出可安装的 Mock IPA。
 */
#include "phoneme_ios_bridge.h"
#include "ios_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* phoneME Feature MR2 的启动入口（由 libcldc_vm.a / midp 提供）。
 * 原项目 src/platform/bbk9588/phoneme_app.c 里同样以 extern 声明后调用。 */
extern int runMidlet(int argc, char **argv);
/* 原项目 freestanding 启动代码里的 init array 在 iOS 上由 dyld 自动完成，
 * 用 weak 声明避免 SDK 差异导致链接失败。 */
extern void bda_run_init_array(void) __attribute__((weak));

static J2MEHostCallbacks g_host;
static J2MEConfig        g_cfg;
static char  g_home[1024];
static char  g_res[1024];
static char  g_docs[1024];
static char  g_jar[1024];
static char  g_diag[2048];
static J2MEState g_state = J2ME_STATE_IDLE;
static int   g_volume = 100;
static int   g_muted = 0;
static uint32_t g_frames = 0u;

static void log_line(const char *text) {
    if (g_host.onLog != NULL)
        g_host.onLog(text, g_host.user);
}

static void set_state(J2MEState state) {
    g_state = state;
    if (g_host.onState != NULL)
        g_host.onState(state, g_host.user);
}

static void frame_sink(const uint16_t *fb, int w, int h, void *user) {
    (void)user;
    if (g_host.onFrame != NULL)
        g_host.onFrame(fb, w, h, g_host.user);
}

static void softpad_action(IOSSoftpadAction action, void *user) {
    (void)user;
    switch (action) {
        case IOS_SOFTPAD_ACTION_VOL_DOWN:
            g_volume = g_volume > 10 ? g_volume - 10 : 0;
            break;
        case IOS_SOFTPAD_ACTION_VOL_UP:
            g_volume = g_volume < 90 ? g_volume + 10 : 100;
            break;
        case IOS_SOFTPAD_ACTION_MUTE:
            g_muted = !g_muted;
            break;
        case IOS_SOFTPAD_ACTION_EXIT:
            /* 与原项目一致：发 SHUTDOWN_EVENT 前先让宿主弹「是/否」确认 */
            if (g_host.onExitRequest != NULL)
                g_host.onExitRequest(g_host.user);
            return;
        default:
            return;
    }
    ios_audio_set_volume(g_volume);
    ios_audio_set_muted(g_muted);
    ios_softpad_set_volume(g_volume, g_muted);
}

int phoneme_ios_init(const J2MEConfig *config, const J2MEHostCallbacks *host) {
    if (config == NULL || host == NULL || host->onFrame == NULL)
        return -1;
    memset(&g_host, 0, sizeof(g_host));
    g_host = *host;
    g_cfg = *config;
    snprintf(g_home, sizeof(g_home), "%s", config->homeDir ? config->homeDir : "");
    snprintf(g_res,  sizeof(g_res),  "%s", config->resourceDir ? config->resourceDir : "");
    snprintf(g_docs, sizeof(g_docs), "%s", config->documentsDir ? config->documentsDir : "");
    g_cfg.homeDir = g_home;
    g_cfg.resourceDir = g_res;
    g_cfg.documentsDir = g_docs;
    g_volume = config->masterVolume > 0 ? config->masterVolume : 100;
    g_muted = 0;
    g_frames = 0u;
    snprintf(g_jar, sizeof(g_jar), "%s", "");

    ios_event_queue_reset(&g_j2me_events);
    ios_fb_init();
    ios_text_init(g_res);
    ios_audio_init();
    ios_audio_set_volume(g_volume);
    ios_softpad_init();
    ios_softpad_set_visible(0);
    ios_softpad_set_volume(g_volume, g_muted);
    ios_softpad_set_action_handler(softpad_action, NULL);
    ios_fb_set_sink(frame_sink, NULL);
    set_state(J2ME_STATE_IDLE);
    log_line("phoneME/iOS 桥接就绪：240x320 RGB565 / 3MiB heap");
    return 0;
}

int phoneme_ios_start_jar(const char *jarPathUtf8) {
    IOSJar jar;
    char *argv[8];
    int argc = 0;
    int rc;

    if (jarPathUtf8 == NULL)
        return -1;
    snprintf(g_jar, sizeof(g_jar), "%s", jarPathUtf8);
    set_state(J2ME_STATE_STARTING);

    /* 沿用原项目的启动顺序：先把 JAR 规范化到 RAM（旧 ZIP data descriptor
     * 兼容），再由 phoneME 从 Manifest 发现 MIDlet-1。 */
    if (ios_jar_open(jarPathUtf8, &jar, 0) == 0) {
        char line[256];
        snprintf(line, sizeof(line), "JAR %ld 字节%s MIDlet-1=%s",
                 jar.size, jar.normalized ? "（已规范化）" : "",
                 jar.midlet1[0] ? jar.midlet1 : "(无)");
        log_line(line);
        ios_jar_close(&jar);
    } else {
        log_line("JAR 缓存失败");
        set_state(J2ME_STATE_ERROR);
        return -2;
    }

    /* 原项目 bda_main() 的等价物。注意：
     * - runMidlet 内部会进入 MIDP master mode 事件循环，在 9588 上是同步
     *   跑在固件主线程；iOS 上必须改为「专用线程 + 非阻塞事件泵」，否则会
     *   阻塞主线程导致 UI 冻结。下面保留同步调用形式作为基准实现，
     *   scripts/ios-kernel-overlay/README.md 里给出线程化改造点。 */
    argv[argc++] = (char *)"J2ME";
    argv[argc++] = (char *)"-jar";
    argv[argc++] = g_jar;
    argv[argc] = NULL;

    if (bda_run_init_array != NULL)
        bda_run_init_array();   /* iOS 上通常为空，dyld 已处理 */
    rc = runMidlet(argc, argv);
    if (rc != 0) {
        log_line("runMidlet 返回非零");
        set_state(J2ME_STATE_ERROR);
        return -3;
    }
    set_state(J2ME_STATE_RUNNING);
    return 0;
}

int phoneme_ios_pump(void) {
    IOSNativeEvent event;
    int consumed = 0;
    /* 沿用原项目：每轮最多消费 4 条原始事件，避免输入洪泛饿死 VM */
    while (consumed < 4 && ios_event_pop(&g_j2me_events, &event)) {
        ++consumed;
        /* TODO(phoneME): 把事件投递到 MIDP eventqueue：
         *   pointer -> midp_store_pointer_event / StoreMIDPEvent
         *   key     -> midp_store_key_event / game action 映射
         * 具体符号取决于 romize 后 midp/src/events/eventqueue_port 的实现，
         * 首次在 macOS runner 编译时按 BUILD_FAILURE.md 日志对齐。 */
    }
    ++g_frames;
    ios_audio_pump();
    /* Java Canvas.paint()/GameCanvas.flushGraphics() 已由 fb_port 直接写进
     * ios_fb_buffer()；这里只负责合成软键盘并提交最终帧。
     * MIDlet 主动退出时应返回 <0，由 MIDP 侧 SHUTDOWN_EVENT 驱动，
     * 首次集成在 macOS runner 上对齐符号后接入。 */
    ios_fb_present();
    return 0;
}

void phoneme_ios_post_input(int type, int a, int b) {
    uint32_t now = ios_j2me_now_ms();
    if (type == J2ME_INPUT_POINTER_DOWN ||
        type == J2ME_INPUT_POINTER_DRAG ||
        type == J2ME_INPUT_POINTER_UP) {
        if (!ios_softpad_handle_touch((J2MEInputType)type, a, b, now))
            ios_event_push(&g_j2me_events, (J2MEInputType)type, a, b, now);
    } else {
        ios_event_push_key(&g_j2me_events, (J2MEInputType)type, a, now);
    }
}

void phoneme_ios_set_volume(int percent) {
    g_volume = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    ios_audio_set_volume(g_volume);
    ios_softpad_set_volume(g_volume, g_muted);
}
int  phoneme_ios_volume(void) { return g_volume; }
void phoneme_ios_set_muted(int muted) {
    g_muted = muted ? 1 : 0;
    ios_audio_set_muted(g_muted);
    ios_softpad_set_volume(g_volume, g_muted);
}
void phoneme_ios_set_keypad_visible(int v) { ios_softpad_set_visible(v); }
int  phoneme_ios_keypad_visible(void) { return ios_softpad_visible(); }

void phoneme_ios_shutdown(void) {
    ios_audio_stop();
    ios_audio_shutdown();
    ios_softpad_shutdown();
    ios_text_shutdown();
    ios_fb_shutdown();
    set_state(J2ME_STATE_STOPPED);
}

int phoneme_ios_state(void) { return (int)g_state; }

const char *phoneme_ios_diagnostics(void) {
    snprintf(g_diag, sizeof(g_diag),
        "{\n"
        "  \"kernel\": \"phoneME Feature MR2 / KVM\",\n"
        "  \"state\": %d,\n"
        "  \"frames\": %u,\n"
        "  \"jar\": \"%s\",\n"
        "  \"java_heap_bytes\": %u,\n"
        "  \"alloc_limit_bytes\": %u,\n"
        "  \"volume\": %d,\n"
        "  \"muted\": %d,\n"
        "  \"softpad_toggles\": %u,\n"
        "  \"softpad_key_events\": %u,\n"
        "  \"softpad_drags\": %u,\n"
        "  \"raw_events_dropped\": %u,\n"
        "  \"raw_moves_coalesced\": %u,\n"
        "  \"audio_write_bytes\": %u\n"
        "}\n",
        (int)g_state, g_frames, g_jar,
        J2ME_DEFAULT_JAVA_HEAP_BYTES, J2ME_DEFAULT_ALLOC_LIMIT,
        g_volume, g_muted,
        ios_softpad_toggle_count(), ios_softpad_key_event_count(),
        ios_softpad_drag_count(),
        g_j2me_events.dropped, g_j2me_events.move_coalesced,
        ios_audio_write_bytes());
    return g_diag;
}

const char *phoneme_ios_version(void) {
    return "phoneME Feature MR2 / KVM (arm64-apple-ios15.0) "
           "+ BBK9588 patch b23";
}
