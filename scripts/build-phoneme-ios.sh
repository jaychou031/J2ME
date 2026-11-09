#!/usr/bin/env bash
# 把 phoneME Feature MR2 + BBK9588 补丁 + iOS overlay 编译成
# arm64-apple-ios15.0 静态库 libphoneme_ios.a。
#
# 必须在装有 Xcode 的 macOS runner 上运行（GitHub Actions macos-14 / macos-15）。
# 这个仓库的 App 默认使用内置诊断内核，本脚本成功与否不影响 IPA 产出：
# CI 对这一步设置了 continue-on-error，失败会生成 BUILD_FAILURE.md 并降级。
#
# 用法：
#   ./scripts/bootstrap-phoneme.sh
#   ./scripts/build-phoneme-ios.sh
#   PHONEME_PREBUILT_LIB=/path/to/arm64-lib.a ./scripts/build-phoneme-ios.sh
#       （若你已在别处编译好 phoneME 核心，直接跳过阶段 4 参与打包）
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS="$ROOT/.deps"
PHONEME="$DEPS/phoneme"
OVERLAY="$ROOT/scripts/ios-kernel-overlay"
OUT="$ROOT/build/kernel"
OBJ="$OUT/obj"
LIB="$OUT/lib/libphoneme_ios.a"
REPORT="$OUT/BUILD_FAILURE.md"
TARGET="arm64-apple-ios15.0"
DEPLOY="15.0"

log()  { printf '\n[kernel] %s\n' "$*"; }
stage(){ printf '\n===== [stage %s] %s =====\n' "$1" "$2"; }
fail() {
    printf '\n[kernel][FATAL] %s\n' "$*" >&2
    {
        echo "# phoneME arm64-iOS 内核构建失败报告"
        echo
        echo "生成时间：$(date -u '+%Y-%m-%d %H:%M:%S UTC')"
        echo "失败原因：\`$*\`"
        echo
        echo "## 环境"
        echo '```'
        echo "runner:  $(uname -a)"
        echo "xcode:   $(xcodebuild -version 2>/dev/null | tr '\n' ' ' || echo unknown)"
        echo "clang:   $(xcrun --sdk iphoneos clang --version 2>/dev/null | head -1 || echo unknown)"
        echo "sdk:     ${IOS_SDK:-unknown}"
        echo "target:  $TARGET"
        echo "phoneme: $(git -C "$PHONEME" rev-parse HEAD 2>/dev/null || echo missing)"
        echo '```'
        echo
        echo "完整构建日志见本次 Actions run 的 \`Build phoneME kernel\` 步骤输出。"
        echo "排错指引：\`scripts/ios-kernel-overlay/README.md\` 与 \`docs/BUILD.md\`。"
    } > "$REPORT" 2>/dev/null || true
    exit 1
}

mkdir -p "$OBJ" "$OUT/lib"

# ---------------------------------------------------------------- 1/6 环境
stage "1/6" "检查 Xcode / iPhoneOS SDK / JDK"
command -v xcrun >/dev/null 2>&1 || fail "未找到 xcrun，请使用 macOS runner"
IOS_SDK="$(xcrun --sdk iphoneos --show-sdk-path 2>/dev/null || true)"
[ -d "$IOS_SDK" ] || fail "iPhoneOS SDK 不可用"
CLANG=(xcrun --sdk iphoneos clang)
[ -d "$PHONEME/cldc" ] || fail "phoneME 源码不存在，请先运行 scripts/bootstrap-phoneme.sh"
JDK_VERSION="$(/usr/libexec/java_home -V 2>&1 | head -3 || true)"
log "SDK     : $IOS_SDK"
log "JDK     : ${JDK_VERSION:-（未检测到，romize 阶段可能需要）}"

COMMON_CFLAGS=(
    -target "$TARGET" -isysroot "$IOS_SDK"
    "-miphoneos-version-min=$DEPLOY"
    -fPIC -O2 -fno-common
    -Wno-everything
    -DJ2ME_PHONEME_INTERNAL_ROM=1
    -DJ2ME_USE_PHONEME_KERNEL=1
    -I"$ROOT/ios/Platform"
    -I"$ROOT/ios/Kernel"
    -I"$DEPS/ios-overlay/include"
    -I"$OVERLAY"
    -I"$PHONEME/cldc/src/vm/share/natives"
    -I"$PHONEME/cldc/src/vm/share/runtime"
    -I"$PHONEME/cldc/src/vm/cpu/c"
    -I"$PHONEME/cldc/src/vm/os/generic"
    -I"$PHONEME/pcsl/types"
    -I"$PHONEME/pcsl/types/darwin_ios_arm64"
    -I"$PHONEME/pcsl/file"
    -I"$PHONEME/pcsl/memory"
    -I"$PHONEME/pcsl/string"
    -I"$PHONEME/pcsl/string/utf16"
    -I"$PHONEME/midp/src/core/kni_util/include"
    -I"$PHONEME/midp/src/core/global_status/include"
    -I"$PHONEME/midp/src/core/string/include"
    -I"$PHONEME/midp/src/core/memory/include"
    -I"$PHONEME/midp/src/core/log_base/include"
    -I"$PHONEME/midp/src/core/vm_services/include"
    -I"$PHONEME/midp/src/ams/ams_base/include"
    -I"$PHONEME/midp/src/events/mastermode_port/include"
    -I"$PHONEME/midp/src/events/eventqueue/include"
    -I"$PHONEME/midp/src/events/eventqueue_port/include"
    -I"$PHONEME/midp/src/highlevelui/keymap/include"
    -I"$PHONEME/midp/src/highlevelui/fb_application/include"
    -I"$PHONEME/midp/src/lowlevelui/graphics/include"
    -I"$PHONEME/midp/src/lowlevelui/graphics_api/include"
    -I"$PHONEME/midp/src/lowlevelui/graphics_util/include"
    -I"$PHONEME/midp/src/lowlevelui/graphics/putpixel/include"
    -I"$PHONEME/midp/src/lowlevelui/graphics/putpixel/native"
)

# ---------------------------------------------------------------- 2/6 字库
stage "2/6" "从原项目 unifont-cjk-14x14.bin 生成 CJK 数据对象"
FONT="$ROOT/ios/Resources/unifont-cjk-14x14.bin"
[ -f "$FONT" ] || fail "缺少字库 $FONT"
python3 - "$FONT" "$OBJ/cjk_font_data.c" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
data = open(src, 'rb').read()
assert data[:4] == b'CJ14', 'CJK 字库 magic 不匹配'
with open(dst, 'w') as out:
    out.write('/* 自动生成：原项目 assets/runtime/unifont-cjk-14x14.bin 的 C 数据形式。\n'
              ' * iOS 链接器不支持 objcopy -I binary，因此改为 C 数组；字节内容完全一致。 */\n'
              '#include <stddef.h>\n'
              'const unsigned char _binary_phoneme_cjk_font_bin_start[] = {\n')
    for i in range(0, len(data), 24):
        out.write('    ' + ','.join('0x%02x' % b for b in data[i:i+24]) + ',\n')
    out.write('};\n')
    out.write('const unsigned long _binary_phoneme_cjk_font_bin_size = %dUL;\n' % len(data))
print('CJK bytes =', len(data))
PY
# 原 phoneme_cjk_text.c 用 (end - start) 求长度；C 数组形式改成显式 size，
# 只动两行 extern 声明，绘制算法一个字节都不改。
sed -e 's|extern const unsigned char _binary_phoneme_cjk_font_bin_end\[\];|extern const unsigned long _binary_phoneme_cjk_font_bin_size;\n#define _binary_phoneme_cjk_font_bin_end (_binary_phoneme_cjk_font_bin_start + _binary_phoneme_cjk_font_bin_size)|' \
    "$ROOT/vendor/phoneme_cjk_text.c.orig" > "$OBJ/cjk_text_patched.c"
log "CJK 数据对象已生成（28400 字形，766812 字节）"

# ---------------------------------------------------------------- 3/6 overlay
stage "3/6" "编译 iOS overlay 与原项目可复用 C 源码"
OBJECTS=()
compile_c() {
    local src="$1" name="$2"
    local out="$OBJ/$name.o"
    "${CLANG[@]}" "${COMMON_CFLAGS[@]}" -std=c11 -x c -c "$src" -o "$out" \
        2>> "$OUT/compile.log" || fail "编译失败：$src（详见 build/kernel/compile.log）"
    OBJECTS+=("$out")
}
: > "$OUT/compile.log"
compile_c "$OVERLAY/phoneme_ios_bridge.c"        "phoneme_ios_bridge"
compile_c "$OVERLAY/bda_compat.c"                "bda_compat"
compile_c "$OVERLAY/ios_bbk_compat.c"            "ios_bbk_compat"
compile_c "$OVERLAY/ios_fb_port.c"               "ios_fb_port"
compile_c "$OBJ/cjk_font_data.c"                 "cjk_font_data"
compile_c "$OBJ/cjk_text_patched.c"              "cjk_text_patched"
compile_c "$ROOT/vendor/phoneme_audio.c.orig"    "phoneme_audio"
compile_c "$ROOT/vendor/native_events.c.orig"    "native_events"
log "overlay 对象：${#OBJECTS[@]} 个"

# ---------------------------------------------------------------- 4/6 phoneME 核心
stage "4/6" "生成 ROMImage 并编译 phoneME CLDC/MIDP/PCSL 核心"
if [ -n "${PHONEME_PREBUILT_LIB:-}" ] && [ -f "$PHONEME_PREBUILT_LIB" ]; then
    log "使用外部提供的 arm64 核心库：$PHONEME_PREBUILT_LIB"
    CORE_LIB="$PHONEME_PREBUILT_LIB"
else
    # phoneME Feature MR2 的构建系统使用 .gmk + GNU make，并需要一个主机端
    # Java romizer 生成 64 位 ROMImage.cpp。这里探测常见 make 入口；
    # 具体目标名可用 PHONEME_MAKE_DIR / PHONEME_MAKE_TARGET 覆盖。
    MAKE_DIR="${PHONEME_MAKE_DIR:-}"
    if [ -z "$MAKE_DIR" ]; then
        for candidate in \
            "$PHONEME/midp/build/common" \
            "$PHONEME/cldc/build" \
            "$PHONEME/midp/build" \
            "$PHONEME"; do
            if [ -f "$candidate/GNUmakefile" ] || [ -f "$candidate/Makefile" ]; then
                MAKE_DIR="$candidate"
                break
            fi
        done
    fi
    [ -n "$MAKE_DIR" ] || fail "未找到 phoneME 的 make 入口；请设置 PHONEME_MAKE_DIR 指向含 GNUmakefile 的目录，或提供 PHONEME_PREBUILT_LIB"

    MAKE_TARGET="${PHONEME_MAKE_TARGET:-}"
    MAKE_ARGS=(PCSL_PLATFORM=darwin_ios_arm64
               PCSL_DIR="$PHONEME/pcsl"
               IOS_SDK="$IOS_SDK"
               IOS_OVERLAY_INCLUDE="$DEPS/ios-overlay/include"
               IOS_PLATFORM_INCLUDE="$ROOT/ios/Platform"
               IOS_KERNEL_INCLUDE="$ROOT/ios/Kernel"
               BBK9588_PORT_DIR="$ROOT/vendor")
    if [ -n "$MAKE_TARGET" ]; then MAKE_ARGS+=("$MAKE_TARGET"); fi
    log "make -C $MAKE_DIR ${MAKE_ARGS[*]}"
    if ! make -C "$MAKE_DIR" "${MAKE_ARGS[@]}" >> "$OUT/make.log" 2>&1; then
        cp "$OUT/make.log" "$OUT/PHONEME_MAKE_FAILURE.log" || true
        fail "phoneME make 失败（详见 build/kernel/PHONEME_MAKE_FAILURE.log）；多数情况下是 64 位 ROMImage romize 或输入端口缺失，见 overlay README"
    fi
    CORE_LIB="$(find "$PHONEME" -name '*.a' -newer "$OUT/compile.log" 2>/dev/null | head -1 || true)"
    [ -n "$CORE_LIB" ] || fail "make 成功但未找到产物 .a；请设置 PHONEME_PREBUILT_LIB 明确指定"
    log "核心库：$CORE_LIB"
fi

# ---------------------------------------------------------------- 5/6 打包
stage "5/6" "打包 libphoneme_ios.a"
rm -f "$LIB"
xcrun --sdk iphoneos ar rcs "$LIB" "${OBJECTS[@]}" \
    || fail "ar 打包失败"
if [ -n "${CORE_LIB:-}" ] && [ -f "$CORE_LIB" ] && [ "$CORE_LIB" != "$LIB" ]; then
    # 把核心库的对象合并进来（避免 App 需要链接两个 .a）
    mkdir -p "$OBJ/core" && rm -f "$OBJ/core"/*.o
    ( cd "$OBJ/core" && xcrun --sdk iphoneos ar x "$CORE_LIB" ) \
        || fail "无法展开核心库 $CORE_LIB"
    # shellcheck disable=SC2086
    xcrun --sdk iphoneos ar q "$LIB" "$OBJ"/core/*.o 2>/dev/null || true
    xcrun --sdk iphoneos ranlib "$LIB" || true
fi
log "静态库：$LIB ($(wc -c < "$LIB") bytes)"

# ---------------------------------------------------------------- 6/6 符号校验
stage "6/6" "校验必需符号"
SYMS="$(xcrun --sdk iphoneos nm -g "$LIB" 2>/dev/null || true)"
MISSING=()
for sym in phoneme_ios_init phoneme_ios_start_jar phoneme_ios_pump \
           phoneme_ios_post_input phoneme_ios_diagnostics \
           runMidlet Java_javax_microedition_media_BBK9588Player_nativeStart \
           initScreenBuffer refreshScreenNormal gx_draw_chars; do
    if ! printf '%s\n' "$SYMS" | grep -q "$sym"; then
        MISSING+=("$sym")
    fi
done
if [ ${#MISSING[@]} -gt 0 ]; then
    printf '%s\n' "$SYMS" > "$OUT/symbols.txt"
    fail "静态库缺少符号：${MISSING[*]}（完整符号表见 build/kernel/symbols.txt）"
fi
rm -f "$REPORT"
log "内核构建成功，App 可用 KERNEL_BACKEND=phoneme 链接。"
