/*
 * BDA 固件服务的 iOS 兼容实现
 *
 * 让 vendor/phoneme_audio.c.orig（原项目 26 KB 的完整 MMAPI：SMF 0/1 MIDI、
 * PCM WAV、playTone、24 声波合成器）几乎不改一行就能编译进 iOS 内核：
 *   bda_audio_write  -> ios_audio_write() -> AudioQueue
 *   bda_gui_millisecond_count -> ios_j2me_now_ms()
 */
#include "bda_audio.h"
#include "bda_time.h"
#include "ios_platform.h"

#include <string.h>

static u32 g_attenuation = BDA_AUDIO_ATTENUATION_FULL_SCALE;

int bda_audio_ready(void) {
    /* 一个 1024-byte 块 = 512 帧；环形缓冲有整块空间才算 ready */
    return ios_audio_ready() >= (int)(IOS_AUDIO_BLOCK_BYTES / 2) ? 1 : 0;
}

int bda_audio_write(const void *data, unsigned long size) {
    int frames = (int)(size / 2u);
    if (data == NULL || frames <= 0)
        return 0;
    return ios_audio_write((const int16_t *)data, frames) * 2;
}

void bda_audio_stop(void) {
    ios_audio_stop();
}

u32 bda_audio_get_attenuation(void) {
    return g_attenuation;
}

void bda_audio_set_attenuation(u32 value) {
    g_attenuation = value;
}

void bda_audio_open_pcm(int sample_rate, int bits, int channels) {
    (void)sample_rate;
    (void)bits;
    (void)channels;
    ios_audio_init();
    ios_audio_pump();
}

u32 bda_gui_millisecond_count(void) {
    return (u32)ios_j2me_now_ms();
}
