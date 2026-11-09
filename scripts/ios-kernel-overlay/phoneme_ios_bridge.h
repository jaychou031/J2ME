/*
 * libphoneme_ios.a 需要导出的 C API
 *
 * 由 scripts/build-phoneme-ios.sh 编译进静态库；
 * ios/Kernel/PhoneME/j2me_phoneme_kernel.c 把 j2me_kernel.h 的 ABI 转发到这里。
 */
#ifndef PHONEME_IOS_BRIDGE_H
#define PHONEME_IOS_BRIDGE_H

#include "j2me_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

int  phoneme_ios_init(const J2MEConfig *config, const J2MEHostCallbacks *host);
int  phoneme_ios_start_jar(const char *jarPathUtf8);
int  phoneme_ios_pump(void);
void phoneme_ios_post_input(int type, int a, int b);
void phoneme_ios_set_volume(int percent);
int  phoneme_ios_volume(void);
void phoneme_ios_set_muted(int muted);
void phoneme_ios_set_keypad_visible(int visible);
int  phoneme_ios_keypad_visible(void);
void phoneme_ios_shutdown(void);
int  phoneme_ios_state(void);
const char *phoneme_ios_diagnostics(void);
const char *phoneme_ios_version(void);

#ifdef __cplusplus
}
#endif

#endif /* PHONEME_IOS_BRIDGE_H */
