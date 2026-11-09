#!/usr/bin/env bash
# 在 macOS runner 上生成 Xcode 工程并编译未签名 .app
# KERNEL_BACKEND=mock（默认，一定成功）或 phoneme（需要 libphoneme_ios.a）
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KERNEL_BACKEND="${KERNEL_BACKEND:-mock}"
CONFIGURATION="${CONFIGURATION:-Release}"
BUILD="$ROOT/build"
DERIVED="$BUILD/DerivedData"
PROJECT="$ROOT/ios/BBK9588J2ME.xcodeproj"
APP_OUT="$BUILD/BBK9588J2ME.app"

log() { printf '\n[app] %s\n' "$*"; }

command -v xcodegen >/dev/null 2>&1 || {
    echo "[app] xcodegen 未安装，尝试 brew install xcodegen" >&2
    brew install xcodegen
}

log "生成 Xcode 工程（XcodeGen）"
xcodegen generate --spec "$ROOT/ios/project.yml" --project "$PROJECT"

EXTRA_SETTINGS=()
if [ "$KERNEL_BACKEND" = "phoneme" ]; then
    CORE="$BUILD/kernel/lib/libphoneme_ios.a"
    if [ ! -f "$CORE" ]; then
        echo "[app] 未找到 $CORE，回退到诊断内核（CI 的 continue-on-error 场景）" >&2
        KERNEL_BACKEND="mock"
    else
        EXTRA_SETTINGS+=(
            "J2ME_USE_PHONEME_KERNEL=1"
            "J2ME_EXTRA_LDFLAGS=-lphoneme_ios"
            "J2ME_EXTRA_LIB_SEARCH_PATHS=$BUILD/kernel/lib"
            "J2ME_EXTRA_HEADER_SEARCH_PATHS=$ROOT/.deps/ios-overlay/include"
        )
        log "链接 phoneME arm64 内核"
    fi
fi
log "内核后端：$KERNEL_BACKEND"

rm -rf "$APP_OUT"
mkdir -p "$BUILD"

log "xcodebuild ($CONFIGURATION / iphoneos / iOS 15.0)"
# macOS 自带 bash 3.2 在 `set -u` 下展开空数组会报 unbound variable，
# 因此把参数先收集到永远非空的数组里再一次性传入。
XCB_ARGS=(
    -project "$PROJECT"
    -scheme BBK9588J2ME
    -configuration "$CONFIGURATION"
    -sdk iphoneos
    -destination 'generic/platform=iOS'
    -derivedDataPath "$DERIVED"
    CODE_SIGNING_ALLOWED=NO
    CODE_SIGNING_REQUIRED=NO
    CODE_SIGN_IDENTITY=
    IPHONEOS_DEPLOYMENT_TARGET=15.0
    clean build
)
if [ ${#EXTRA_SETTINGS[@]} -gt 0 ]; then
    XCB_ARGS+=( "${EXTRA_SETTINGS[@]}" )
fi
xcodebuild "${XCB_ARGS[@]}"

BUILT="$DERIVED/Build/Products/$CONFIGURATION-iphoneos/BBK9588J2ME.app"
[ -d "$BUILT" ] || { echo "[app] 找不到产物 $BUILT" >&2; exit 1; }
cp -R "$BUILT" "$APP_OUT"

log "产物：$APP_OUT"
log "最低系统：$(/usr/libexec/PlistBuddy -c 'Print :MinimumOSVersion' "$APP_OUT/Info.plist" 2>/dev/null || echo '见 Info.plist')"
log "架构：$(lipo -archs "$APP_OUT/BBK9588J2ME" 2>/dev/null || echo unknown)"
