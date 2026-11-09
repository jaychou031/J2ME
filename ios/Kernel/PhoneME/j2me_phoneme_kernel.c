/*
 * phoneME 内核桥接（J2ME_USE_PHONEME_KERNEL=1 时编译）
 *
 * 这个文件本身不实现 J2ME，只把 j2me_kernel.h 的 ABI 转发给
 * libphoneme_ios.a —— 后者由 scripts/build-phoneme-ios.sh 在 GitHub Actions
 * 的 macOS runner 上，用上游 magicus/phoneME 固定 commit + 原仓库的
 * BBK9588 补丁重新编译成 arm64-apple-ios 静态库。
 *
 * 为什么必须重编译而不能复用仓库里的预构建对象：
 *   third_party/phoneme/prebuilt 下的 .a 归档是 MIPS32 ELF 可重定位对象，
 *   ROMImage.o 里的指针是 32 位 MIPS 重定位，与 arm64 的 ABI 完全不兼容。
 *   ROMImage 必须由 romizer 在 macOS 上从 Java 类库重新生成。
 *
 * 静态库需要导出的 C 符号见 scripts/ios-kernel-overlay/phoneme_ios_bridge.h。
 */
#include "j2me_kernel.h"

#if J2ME_USE_PHONEME_KERNEL

#include "phoneme_ios_bridge.h"

static int pm_init(const J2MEConfig *c, const J2MEHostCallbacks *h) {
    return phoneme_ios_init(c, h);
}
static int pm_startJar(const char *p)          { return phoneme_ios_start_jar(p); }
static int pm_pump(void)                       { return phoneme_ios_pump(); }
static void pm_postInput(J2MEInputType t, int a, int b) {
    phoneme_ios_post_input((int)t, a, b);
}
static void pm_setVolume(int v)                { phoneme_ios_set_volume(v); }
static int  pm_volume(void)                    { return phoneme_ios_volume(); }
static void pm_setMuted(int m)                 { phoneme_ios_set_muted(m); }
static void pm_setKeypadVisible(int v)         { phoneme_ios_set_keypad_visible(v); }
static int  pm_keypadVisible(void)             { return phoneme_ios_keypad_visible(); }
static void pm_shutdown(void)                  { phoneme_ios_shutdown(); }
static J2MEState pm_state(void)                { return (J2MEState)phoneme_ios_state(); }
static const char *pm_diagnostics(void)       { return phoneme_ios_diagnostics(); }
static const char *pm_version(void)           { return phoneme_ios_version(); }

static const J2MEKernel kPhoneMEKernel = {
    pm_init,
    pm_startJar,
    pm_pump,
    pm_postInput,
    pm_setVolume,
    pm_volume,
    pm_setMuted,
    pm_setKeypadVisible,
    pm_keypadVisible,
    pm_shutdown,
    pm_state,
    pm_diagnostics,
    pm_version
};

const J2MEKernel *j2me_kernel_get(void) {
    return &kPhoneMEKernel;
}

#endif /* J2ME_USE_PHONEME_KERNEL */
