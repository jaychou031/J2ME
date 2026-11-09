/*
 * 音频输出 —— 对应原项目 phoneme_audio.c 的原生写入端
 *
 * 保持原项目规格：
 *   - 22050 Hz / 16-bit / mono PCM
 *   - 1024-byte 小块非阻塞写入（这里 512 帧 = 1024 字节）
 *   - 0–100% 主音量作为数字比例，静音开关独立
 *   - 写入端不阻塞、不依赖窗口消息或 GUI Timer
 *
 * iOS 端用 AudioToolbox 的 AudioQueue 替代固件 bda_audio。
 * 非 Apple 平台提供纯内存实现，便于在 Linux 上做语法检查与单元测试。
 */
#include "ios_platform.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define RING_FRAMES   (IOS_AUDIO_SAMPLE_RATE * 2)  /* 2 秒 */
#define QUEUE_BUFFERS 3
#define QUEUE_FRAMES  (IOS_AUDIO_BLOCK_BYTES / 2)  /* 512 帧 */

static int16_t    g_ring[RING_FRAMES];
static atomic_uint g_read_pos;
static atomic_uint g_write_pos;
static atomic_uint g_count;
static int        g_volume = 100;
static int        g_muted  = 0;
static int        g_started = 0;
static uint32_t   g_write_calls = 0u;
static uint32_t   g_write_bytes = 0u;
static uint32_t   g_ready_misses = 0u;

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/AudioQueue.h>

static AudioQueueRef       g_queue = NULL;
static AudioQueueBufferRef g_buffers[QUEUE_BUFFERS];

static void audio_callback(void *user, AudioQueueRef queue,
                           AudioQueueBufferRef buffer) {
    UInt32 frames = QUEUE_FRAMES;
    UInt32 i;
    UInt32 consumed = 0u;
    SInt16 *out = (SInt16 *)buffer->mAudioData;
    unsigned int pos = atomic_load(&g_read_pos);
    unsigned int available = atomic_load(&g_count);
    (void)user;
    for (i = 0u; i < frames; ++i) {
        if (consumed < available) {
            out[i] = g_ring[pos];
            pos = (pos + 1u) % RING_FRAMES;
            ++consumed;
        } else {
            out[i] = 0;
        }
    }
    atomic_store(&g_read_pos, pos);
    if (consumed > 0u)
        atomic_fetch_sub(&g_count, consumed);
    buffer->mAudioDataByteSize = (UInt32)(frames * sizeof(SInt16));
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static int queue_start(void) {
    AudioStreamBasicDescription format;
    int i;
    if (g_queue != NULL)
        return 0;
    memset(&format, 0, sizeof(format));
    format.mSampleRate       = (Float64)IOS_AUDIO_SAMPLE_RATE;
    format.mFormatID         = kAudioFormatLinearPCM;
    format.mFormatFlags      = (AudioFormatFlags)(kAudioFormatFlagIsSignedInteger |
                                                  kAudioFormatFlagIsPacked);
    format.mChannelsPerFrame = 1;
    format.mBitsPerChannel   = 16;
    format.mFramesPerPacket  = 1;
    format.mBytesPerFrame    = 2;
    format.mBytesPerPacket   = 2;
    if (AudioQueueNewOutput(&format, audio_callback, NULL, NULL,
                            kCFRunLoopCommonModes, 0, &g_queue) != noErr) {
        g_queue = NULL;
        return -1;
    }
    for (i = 0; i < QUEUE_BUFFERS; ++i) {
        g_buffers[i] = NULL;
        if (AudioQueueAllocateBuffer(g_queue, QUEUE_FRAMES * sizeof(SInt16),
                                     &g_buffers[i]) != noErr) {
            AudioQueueDispose(g_queue, true);
            g_queue = NULL;
            return -1;
        }
        audio_callback(NULL, g_queue, g_buffers[i]);
    }
    if (AudioQueueStart(g_queue, NULL) != noErr) {
        AudioQueueDispose(g_queue, true);
        g_queue = NULL;
        return -1;
    }
    return 0;
}
#endif

int ios_audio_init(void) {
    memset(g_ring, 0, sizeof(g_ring));
    atomic_store(&g_read_pos, 0u);
    atomic_store(&g_write_pos, 0u);
    atomic_store(&g_count, 0u);
    g_volume = 100;
    g_muted = 0;
    g_started = 0;
    g_write_calls = 0u;
    g_write_bytes = 0u;
    g_ready_misses = 0u;
    return 0;
}

void ios_audio_shutdown(void) {
#if defined(__APPLE__)
    if (g_queue != NULL) {
        AudioQueueStop(g_queue, true);
        AudioQueueDispose(g_queue, true);
        g_queue = NULL;
    }
#endif
    g_started = 0;
}

void ios_audio_pump(void) {
#if defined(__APPLE__)
    if (!g_started && g_queue == NULL) {
        if (queue_start() == 0)
            g_started = 1;
    }
#else
    g_started = 1;
#endif
}

int ios_audio_ready(void) {
    unsigned int used = atomic_load(&g_count);
    if (used >= RING_FRAMES - 1u)
        return 0;
    return (int)(RING_FRAMES - 1u - used);
}

int ios_audio_write(const int16_t *samples, int frames) {
    int i;
    int written = 0;
    unsigned int pos;
    unsigned int used;
    if (samples == NULL || frames <= 0)
        return 0;
    ++g_write_calls;
    pos = atomic_load(&g_write_pos);
    used = atomic_load(&g_count);
    for (i = 0; i < frames; ++i) {
        int32_t value;
        if (used + (unsigned int)written >= RING_FRAMES - 1u) {
            ++g_ready_misses;
            break;
        }
        value = (int32_t)samples[i];
        if (g_muted)
            value = 0;
        else
            value = value * g_volume / 100;
        if (value > 32767) value = 32767;
        if (value < -32768) value = -32768;
        g_ring[pos] = (int16_t)value;
        pos = (pos + 1u) % RING_FRAMES;
        ++written;
    }
    /* 先发布数据位置，再发布计数，保证音频回调不会读到半写状态 */
    atomic_store(&g_write_pos, pos);
    if (written > 0)
        atomic_fetch_add(&g_count, (unsigned int)written);
    g_write_bytes += (uint32_t)(written * 2);
    ios_audio_pump();
    return written;
}

void ios_audio_stop(void) {
    unsigned int pos = atomic_load(&g_write_pos);
    atomic_store(&g_count, 0u);
    atomic_store(&g_read_pos, pos);
}

void ios_audio_set_volume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    g_volume = percent;
}

int ios_audio_volume(void) { return g_volume; }

void ios_audio_set_muted(int muted) { g_muted = muted ? 1 : 0; }

int ios_audio_muted(void) { return g_muted; }

/* 诊断计数，对应原项目 J2MEPHONE.LOG 的 AUDIO_WRITE_* / READY_MISSES */
uint32_t ios_audio_write_count(void)  { return g_write_calls; }
uint32_t ios_audio_write_bytes(void)  { return g_write_bytes; }
uint32_t ios_audio_ready_misses(void) { return g_ready_misses; }
