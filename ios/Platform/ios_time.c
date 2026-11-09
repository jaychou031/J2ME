/* 非 Apple 平台在严格 -std=c11 下需要显式打开 POSIX 接口 */
#if !defined(__APPLE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

/*
 * 单调毫秒时钟 —— 对应原项目 phoneme_time_ms()
 * iOS 上用 mach_absolute_time（不受系统时间修改影响），其它 POSIX 平台降级到
 * clock_gettime(CLOCK_MONOTONIC)，方便在 Linux 上做语法/单元测试。
 */
#include "ios_platform.h"

#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif
#include <time.h>
#include <unistd.h>

uint32_t ios_j2me_now_ms(void) {
#if defined(__APPLE__)
    static mach_timebase_info_data_t timebase = {0u, 0u};
    uint64_t ticks;
    if (timebase.denom == 0u) {
        (void)mach_timebase_info(&timebase);
        if (timebase.denom == 0u) {
            timebase.numer = 1u;
            timebase.denom = 1u;
        }
    }
    ticks = mach_absolute_time();
    return (uint32_t)((ticks * (uint64_t)timebase.numer /
                       (uint64_t)timebase.denom) / 1000000ULL);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0u;
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u +
                      (uint64_t)ts.tv_nsec / 1000000u);
#endif
}

void ios_j2me_sleep_ms(uint32_t ms) {
    struct timespec request;
    struct timespec remaining;
    if (ms == 0u)
        return;
    request.tv_sec = (time_t)(ms / 1000u);
    request.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&request, &remaining) != 0)
        request = remaining;
}
