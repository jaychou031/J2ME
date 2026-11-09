/* BDA 音频 API 的 iOS 兼容头：签名对齐 src/platform/bbk9588/phoneme_audio.c */
#ifndef BDA_AUDIO_H
#define BDA_AUDIO_H
#include "bda_types.h"

#define BDA_AUDIO_SAMPLE_RATE_22050        22050
#define BDA_AUDIO_BITS_16                  16
#define BDA_AUDIO_CHANNELS_MONO            1
#define BDA_AUDIO_ATTENUATION_FULL_SCALE   0u

int  bda_audio_ready(void);
int  bda_audio_write(const void *data, unsigned long size);
void bda_audio_stop(void);
u32  bda_audio_get_attenuation(void);
void bda_audio_set_attenuation(u32 value);
void bda_audio_open_pcm(int sample_rate, int bits, int channels);

#endif
