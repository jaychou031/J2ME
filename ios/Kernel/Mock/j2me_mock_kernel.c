/*
 * 内置诊断内核（J2ME_USE_PHONEME_KERNEL=0 时的默认实现）
 *
 * 它不执行 Java 字节码，但完整跑通除「KVM 解释执行」以外的所有链路：
 *   - 240×320 RGB565 帧合成 + 缩放提交
 *   - 原项目 CJK 14×14 字库与西文混排
 *   - 悬浮点 / 半透明软键盘 / 音量条 / 按键事件
 *   - 事件队列（合并、保序、诊断计数）
 *   - JAR 导入、老式 ZIP 规范化、Manifest MIDlet-1 解析
 *   - AudioQueue 22050Hz 输出与主音量
 *
 * 存在意义：你没有 Mac，无法在本地验证 iOS 构建。这个内核让 GitHub Actions
 * 第一次运行就能产出一个真正能装、能看、能点、能验证缩放的 IPA；
 * phoneME 内核（scripts/build-phoneme-ios.sh）作为可插拔的第二阶段接进来，
 * 两者共用同一个 j2me_kernel.h ABI，App 层代码零改动。
 */
#include "j2me_kernel.h"
#include "ios_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if J2ME_USE_PHONEME_KERNEL == 0

#define DIAG_CAP 1024

static J2MEHostCallbacks g_host;
static J2MEConfig        g_cfg;
static char  g_home[1024];
static char  g_res[1024];
static char  g_docs[1024];
static char  g_jarPath[1024];
static char  g_jarName[128];
static char  g_midlet1[128];
static char  g_midletName[128];
static char  g_diag[DIAG_CAP];

static J2MEState g_state = J2ME_STATE_IDLE;
static int       g_volume = 100;
static int       g_muted = 0;
static uint32_t  g_frames = 0u;
static uint32_t  g_start_ms = 0u;
static int       g_last_key = 0;
static int       g_last_x = -1;
static int       g_last_y = -1;
static uint32_t  g_key_events = 0u;
static uint32_t  g_pointer_events = 0u;
static int       g_jar_normalized = 0;

static void log_line(const char *text) {
    if (g_host.onLog != NULL)
        g_host.onLog(text, g_host.user);
}

static void set_state(J2MEState state) {
    g_state = state;
    if (g_host.onState != NULL)
        g_host.onState(state, g_host.user);
}

static void forward_frame(const uint16_t *fb, int w, int h, void *user) {
    (void)user;
    if (g_host.onFrame != NULL)
        g_host.onFrame(fb, w, h, g_host.user);
}

static void on_softpad_action(IOSSoftpadAction action, void *user) {
    (void)user;
    switch (action) {
        case IOS_SOFTPAD_ACTION_VOL_DOWN:
            g_volume -= 10;
            if (g_volume < 0) g_volume = 0;
            break;
        case IOS_SOFTPAD_ACTION_VOL_UP:
            g_volume += 10;
            if (g_volume > 100) g_volume = 100;
            break;
        case IOS_SOFTPAD_ACTION_MUTE:
            g_muted = g_muted ? 0 : 1;
            break;
        case IOS_SOFTPAD_ACTION_EXIT:
            /* 与原项目一致：不立即退出，交给宿主弹「是/否」确认 */
            if (g_host.onExitRequest != NULL)
                g_host.onExitRequest(g_host.user);
            return;
        default:
            return;
    }
    ios_audio_set_volume(g_volume);
    ios_audio_set_muted(g_muted);
    ios_softpad_set_volume(g_volume, g_muted);
    {
        char line[96];
        snprintf(line, sizeof(line), "音量 %d%%%s", g_volume,
                 g_muted ? " (静音)" : "");
        log_line(line);
    }
}

static int mock_init(const J2MEConfig *config, const J2MEHostCallbacks *host) {
    if (config == NULL || host == NULL || host->onFrame == NULL)
        return -1;
    memset(&g_host, 0, sizeof(g_host));
    g_host = *host;
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg = *config;
    snprintf(g_home, sizeof(g_home), "%s", config->homeDir ? config->homeDir : "");
    snprintf(g_res,  sizeof(g_res),  "%s", config->resourceDir ? config->resourceDir : "");
    snprintf(g_docs, sizeof(g_docs), "%s", config->documentsDir ? config->documentsDir : "");
    g_cfg.homeDir = g_home;
    g_cfg.resourceDir = g_res;
    g_cfg.documentsDir = g_docs;
    if (g_cfg.javaHeapBytes == 0u)
        g_cfg.javaHeapBytes = J2ME_DEFAULT_JAVA_HEAP_BYTES;
    if (g_cfg.allocLimitBytes == 0u)
        g_cfg.allocLimitBytes = J2ME_DEFAULT_ALLOC_LIMIT;
    g_volume = config->masterVolume > 0 ? config->masterVolume : 100;
    g_muted = 0;
    g_frames = 0u;
    g_start_ms = ios_j2me_now_ms();
    g_last_key = 0;
    g_last_x = -1;
    g_last_y = -1;
    g_key_events = 0u;
    g_pointer_events = 0u;
    g_jar_normalized = 0;
    snprintf(g_jarPath, sizeof(g_jarPath), "%s", "");
    snprintf(g_jarName, sizeof(g_jarName), "%s", "(未载入)");
    snprintf(g_midlet1, sizeof(g_midlet1), "%s", "-");
    snprintf(g_midletName, sizeof(g_midletName), "%s", "-");

    ios_event_queue_reset(&g_j2me_events);
    ios_fb_init();
    if (ios_text_init(g_res) != 0) {
        log_line("警告：CJK 字库未加载，中文将显示为方块");
    }
    ios_audio_init();
    ios_audio_set_volume(g_volume);
    ios_audio_set_muted(g_muted);
    ios_softpad_init();
    ios_softpad_set_visible(0);
    ios_softpad_set_volume(g_volume, g_muted);
    ios_softpad_set_action_handler(on_softpad_action, NULL);
    ios_fb_set_sink(forward_frame, NULL);

    set_state(J2ME_STATE_IDLE);
    log_line("诊断内核就绪：240x320 RGB565 / CJK 28400 字形 / 22kHz PCM");
    return 0;
}

static int mock_startJar(const char *jarPathUtf8) {
    IOSJar jar;
    const char *leaf;
    set_state(J2ME_STATE_STARTING);
    snprintf(g_jarPath, sizeof(g_jarPath), "%s",
             jarPathUtf8 ? jarPathUtf8 : "");
    leaf = strrchr(g_jarPath, '/');
    leaf = leaf ? leaf + 1 : g_jarPath;
    snprintf(g_jarName, sizeof(g_jarName), "%s", leaf);
    snprintf(g_midlet1, sizeof(g_midlet1), "%s", "-");
    snprintf(g_midletName, sizeof(g_midletName), "%s", "-");
    g_jar_normalized = 0;
    if (jarPathUtf8 != NULL && ios_jar_open(jarPathUtf8, &jar, 0) == 0) {
        g_jar_normalized = jar.normalized;
        snprintf(g_midlet1, sizeof(g_midlet1), "%s",
                 jar.midlet1[0] ? jar.midlet1 : "(Manifest 无 MIDlet-1)");
        snprintf(g_midletName, sizeof(g_midletName), "%s",
                 jar.midletName[0] ? jar.midletName : "-");
        {
            char line[256];
            snprintf(line, sizeof(line),
                     "JAR %ld 字节%s，MIDlet-1=%s",
                     jar.size, jar.normalized ? "（已做旧 ZIP 规范化）" : "",
                     g_midlet1);
            log_line(line);
        }
        ios_jar_close(&jar);
    } else {
        log_line("JAR 打开失败或该文件不是有效 ZIP");
    }
    set_state(J2ME_STATE_RUNNING);
    /* 诊断内核不执行 Java：发出一声短提示音，验证音频链路 */
    {
        int16_t tone[512];
        int i;
        for (i = 0; i < 512; ++i)
            tone[i] = (int16_t)((i & 32) ? 2200 : -2200);
        ios_audio_write(tone, 512);
    }
    return 0;
}

static void draw_background(void) {
    int i;
    ios_fb_clear(IOS_RGB565(16, 22, 34));
    ios_fb_fill_rect(0, 0, 240, 17, IOS_RGB565(38, 52, 78));
    ios_text_draw("J2ME  240x320  RGB565", 5, 1, IOS_RGB565(220, 230, 245));
    ios_fb_hline(0, 17, 240, IOS_RGB565(70, 90, 130));
    /* 触摸轨迹区的网格 */
    ios_fb_fill_rect(8, 150, 224, 76, IOS_RGB565(20, 28, 44));
    ios_fb_outline_rect(8, 150, 224, 76, IOS_RGB565(60, 78, 110), 1);
    for (i = 1; i < 8; ++i)
        ios_fb_vline(8 + i * 28, 150, 76, IOS_RGB565(30, 40, 62));
    for (i = 1; i < 3; ++i)
        ios_fb_hline(8, 150 + i * 25, 224, IOS_RGB565(30, 40, 62));
}

static void draw_diagnostic_frame(void) {
    char line[160];
    const uint16_t kTitle  = IOS_RGB565(255, 214, 90);
    const uint16_t kLabel  = IOS_RGB565(130, 150, 185);
    const uint16_t kValue  = IOS_RGB565(225, 232, 245);
    const uint16_t kHint   = IOS_RGB565(120, 200, 150);

    draw_background();

    ios_text_draw_centered("BBK 9588 · J2ME / iOS", 120, 22, kTitle);
    ios_fb_hline(16, 40, 208, IOS_RGB565(70, 90, 130));

    ios_text_draw("内核", 10, 46, kLabel);
    ios_text_draw("诊断内核（未链接 phoneME）", 48, 46, kValue);

    ios_text_draw("文件", 10, 62, kLabel);
    ios_text_draw_clipped(g_jarName, 48, 62, 182, kValue);

    ios_text_draw("入口", 10, 78, kLabel);
    ios_text_draw_clipped(g_midlet1, 48, 78, 182, kValue);

    ios_text_draw("名称", 10, 94, kLabel);
    ios_text_draw_clipped(g_midletName, 48, 94, 182, kValue);

    ios_text_draw("堆", 10, 110, kLabel);
    snprintf(line, sizeof(line), "%u KiB  /  上限 %u KiB",
             g_cfg.javaHeapBytes / 1024u, g_cfg.allocLimitBytes / 1024u);
    ios_text_draw(line, 48, 110, kValue);

    ios_text_draw("触摸轨迹（点击任意位置）", 10, 132, kLabel);
    if (g_last_x >= 0 && g_last_y >= 0) {
        int cx = 8 + g_last_x * 224 / 240;
        int cy = 150 + g_last_y * 76 / 320;
        ios_fb_fill_rect(cx - 5, cy, 11, 1, kTitle);
        ios_fb_fill_rect(cx, cy - 5, 1, 11, kTitle);
        ios_fb_outline_rect(cx - 3, cy - 3, 7, 7, IOS_RGB565_WHITE, 1);
    }

    snprintf(line, sizeof(line), "帧 %u   按键 %u   触摸 %u",
             g_frames, g_key_events, g_pointer_events);
    ios_text_draw(line, 10, 232, kValue);
    snprintf(line, sizeof(line), "音量 %d%%%s   最后键 %d",
             g_volume, g_muted ? " 静音" : "", g_last_key);
    ios_text_draw(line, 10, 248, kValue);
    snprintf(line, sizeof(line), "队列 丢弃 %u 关键 %u 合并 %u 峰值 %u",
             g_j2me_events.dropped, g_j2me_events.dropped_critical,
             g_j2me_events.move_coalesced, g_j2me_events.peak_depth);
    ios_text_draw_clipped(line, 10, 264, 228, kLabel);

    ios_fb_hline(10, 284, 220, IOS_RGB565(70, 90, 130));
    ios_text_draw("导入 .jar 后启用 phoneME 内核", 10, 290, kHint);
    ios_text_draw("右上圆点展开软键盘", 10, 305, kLabel);
}

static int mock_pump(void) {
    IOSNativeEvent event;
    int consumed = 0;
    /* 沿用原项目：每轮最多消费 4 条原始事件，避免输入洪泛饿死 VM */
    while (consumed < 4 && ios_event_pop(&g_j2me_events, &event)) {
        ++consumed;
        if (event.type == J2ME_INPUT_KEY_DOWN ||
            event.type == J2ME_INPUT_KEY_UP ||
            event.type == J2ME_INPUT_KEY_REPEAT) {
            g_last_key = (int)event.key;
            ++g_key_events;
        } else {
            if (event.type == J2ME_INPUT_POINTER_DOWN ||
                event.type == J2ME_INPUT_POINTER_DRAG) {
                g_last_x = event.x;
                g_last_y = event.y;
            }
            ++g_pointer_events;
        }
    }
    ++g_frames;
    draw_diagnostic_frame();
    ios_audio_pump();
    ios_fb_present();
    return 0;
}

static void mock_postInput(J2MEInputType type, int a, int b) {
    uint32_t now = ios_j2me_now_ms() - g_start_ms;
    if (type == J2ME_INPUT_POINTER_DOWN ||
        type == J2ME_INPUT_POINTER_DRAG ||
        type == J2ME_INPUT_POINTER_UP) {
        int handled = ios_softpad_handle_touch(type, a, b, now);
        if (!handled)
            ios_event_push(&g_j2me_events, type, a, b, now);
        if (type != J2ME_INPUT_POINTER_UP && a >= 0 && b >= 0) {
            g_last_x = a;
            g_last_y = b;
        }
    } else {
        ios_event_push_key(&g_j2me_events, type, a, now);
    }
}

static void mock_setVolume(int percent) {
    g_volume = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    ios_audio_set_volume(g_volume);
    ios_softpad_set_volume(g_volume, g_muted);
}
static int  mock_volume(void) { return g_volume; }
static void mock_setMuted(int muted) {
    g_muted = muted ? 1 : 0;
    ios_audio_set_muted(g_muted);
    ios_softpad_set_volume(g_volume, g_muted);
}
static void mock_setKeypadVisible(int visible) {
    ios_softpad_set_visible(visible);
}
static int  mock_keypadVisible(void) {
    return ios_softpad_visible();
}

static void mock_shutdown(void) {
    ios_audio_stop();
    ios_audio_shutdown();
    ios_softpad_shutdown();
    ios_text_shutdown();
    ios_fb_shutdown();
    set_state(J2ME_STATE_STOPPED);
}

static J2MEState mock_state(void) { return g_state; }

static const char *mock_diagnostics(void) {
    snprintf(g_diag, sizeof(g_diag),
        "{\n"
        "  \"kernel\": \"mock-diagnostic\",\n"
        "  \"state\": %d,\n"
        "  \"frames\": %u,\n"
        "  \"jar\": \"%s\",\n"
        "  \"midlet1\": \"%s\",\n"
        "  \"jar_normalized\": %s,\n"
        "  \"java_heap_bytes\": %u,\n"
        "  \"alloc_limit_bytes\": %u,\n"
        "  \"volume\": %d,\n"
        "  \"muted\": %d,\n"
        "  \"softpad_toggles\": %u,\n"
        "  \"softpad_key_events\": %u,\n"
        "  \"softpad_drags\": %u,\n"
        "  \"softpad_exits\": %u,\n"
        "  \"raw_events_dropped\": %u,\n"
        "  \"raw_events_dropped_critical\": %u,\n"
        "  \"raw_moves_coalesced\": %u,\n"
        "  \"raw_queue_peak\": %u,\n"
        "  \"present_count\": %u,\n"
        "  \"audio_write_calls\": %u,\n"
        "  \"audio_write_bytes\": %u,\n"
        "  \"audio_ready_misses\": %u\n"
        "}\n",
        (int)g_state, g_frames, g_jarName, g_midlet1,
        g_jar_normalized ? "true" : "false",
        g_cfg.javaHeapBytes, g_cfg.allocLimitBytes,
        g_volume, g_muted,
        ios_softpad_toggle_count(), ios_softpad_key_event_count(),
        ios_softpad_drag_count(), ios_softpad_exit_count(),
        g_j2me_events.dropped, g_j2me_events.dropped_critical,
        g_j2me_events.move_coalesced, g_j2me_events.peak_depth,
        ios_fb_present_count(),
        ios_audio_write_count(), ios_audio_write_bytes(),
        ios_audio_ready_misses());
    return g_diag;
}

static const char *mock_version(void) {
    return "mock-diagnostic/0.1.0  (phoneME 内核未链接；"
           "在 GitHub Actions 上启用 KERNEL_BACKEND=phoneme 后切换)";
}

static const J2MEKernel kMockKernel = {
    mock_init,
    mock_startJar,
    mock_pump,
    mock_postInput,
    mock_setVolume,
    mock_volume,
    mock_setMuted,
    mock_setKeypadVisible,
    mock_keypadVisible,
    mock_shutdown,
    mock_state,
    mock_diagnostics,
    mock_version
};

const J2MEKernel *j2me_kernel_get(void) {
    return &kMockKernel;
}

#endif /* J2ME_USE_PHONEME_KERNEL == 0 */
