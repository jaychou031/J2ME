/*
 * 补丁 runMidlet.c / JVM_generic.cpp 引用的两个平台时钟函数。
 * 原项目由 phoneme_bbk9588_port.c 提供，iOS 上转发到 ios_time.c。
 */
#include "ios_platform.h"

unsigned int j2me_phoneme_bbk9588_clock_ms(void) {
    return (unsigned int)ios_j2me_now_ms();
}

void j2me_phoneme_bbk9588_sleep_ms(unsigned int milliseconds) {
    ios_j2me_sleep_ms((uint32_t)milliseconds);
}

/* 以下是原项目 phoneme_bbk9588_port.c 导出、可能被补丁代码引用的诊断符号。
 * iOS 内核不编译那个文件（内含固件 bda_gui_* 依赖），在这里给出等价实现。 */
int j2me_phoneme_bbk9588_platform_initialize(void) {
    ios_event_queue_reset(&g_j2me_events);
    return 0;
}

void j2me_phoneme_bbk9588_platform_finalize(void) {
    ios_audio_stop();
}

int j2me_phoneme_bbk9588_is_detached(void) { return 0; }
int j2me_phoneme_bbk9588_take_exit_request(void) { return 0; }

const char *j2me_phoneme_bbk9588_resolve_path(const char *path) {
    /* iOS 沙箱内路径本来就是 UTF-8，无需像 9588 那样做 GBK 别名映射 */
    return path;
}

unsigned int j2me_phoneme_bbk9588_present_count(void) {
    return ios_fb_present_count();
}

unsigned int j2me_phoneme_bbk9588_softpad_toggle_count(void) {
    return ios_softpad_toggle_count();
}

unsigned int j2me_phoneme_bbk9588_softpad_key_event_count(void) {
    return ios_softpad_key_event_count();
}

unsigned int j2me_phoneme_bbk9588_softpad_drag_count(void) {
    return ios_softpad_drag_count();
}

unsigned int j2me_phoneme_bbk9588_softpad_exit_count(void) {
    return ios_softpad_exit_count();
}

unsigned int j2me_phoneme_bbk9588_raw_event_fetch_count(void) {
    return (unsigned int)(ios_softpad_key_event_count() +
                          ios_softpad_toggle_count());
}
