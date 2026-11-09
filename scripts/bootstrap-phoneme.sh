#!/usr/bin/env bash
# 拉取 phoneME Feature MR2 固定 commit，校验并应用 BBK 9588 补丁，
# 再把 iOS overlay 复制进源码树。完全复刻原项目 tools/bootstrap.ps1 的语义。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="$ROOT/vendor/deps.lock.psd1"
PATCH="$ROOT/vendor/phoneme-feature-mr2-b23-bbk9588.patch"
OVERLAY="$ROOT/scripts/ios-kernel-overlay"
DEPS="$ROOT/.deps"
DEST="$DEPS/phoneme"

log() { printf '\n[bootstrap] %s\n' "$*"; }
fail() { printf '\n[bootstrap][FATAL] %s\n' "$*" >&2; exit 1; }

read_field() {
    sed -n "s/.*$1 = '\([^']*\)'.*/\1/p" "$LOCK" | head -1
}
URL="$(read_field Url)"
COMMIT="$(read_field Commit)"
EXPECTED="$(read_field PatchSha256)"

[ -n "$URL" ] && [ -n "$COMMIT" ] && [ -n "$EXPECTED" ] \
    || fail "无法解析 vendor/deps.lock.psd1（Url/Commit/PatchSha256）"
[ -f "$PATCH" ] || fail "补丁不存在：$PATCH"

if command -v shasum >/dev/null 2>&1; then
    ACTUAL="$(shasum -a 256 "$PATCH" | awk '{print $1}' | tr '[:lower:]' '[:upper:]')"
else
    ACTUAL="$(sha256sum "$PATCH" | awk '{print $1}' | tr '[:lower:]' '[:upper:]')"
fi
EXPECTED="$(printf '%s' "$EXPECTED" | tr '[:lower:]' '[:upper:]')"
[ "$ACTUAL" = "$EXPECTED" ] || fail "补丁 SHA-256 不匹配：期望 $EXPECTED，实际 $ACTUAL"
log "补丁校验通过：$ACTUAL"

mkdir -p "$DEPS"
if [ ! -d "$DEST/.git" ]; then
    log "clone $URL"
    git clone --filter=blob:none "$URL" "$DEST"
fi
if ! git -C "$DEST" cat-file -e "${COMMIT}^{commit}" 2>/dev/null; then
    log "fetch 固定 commit $COMMIT"
    git -C "$DEST" fetch --depth 1 origin "$COMMIT"
fi
HEAD="$(git -C "$DEST" rev-parse HEAD 2>/dev/null || true)"
if [ "$HEAD" != "$COMMIT" ]; then
    if [ -n "$(git -C "$DEST" status --porcelain 2>/dev/null || true)" ]; then
        fail "phoneME 工作区有本地改动且不在目标 revision：$HEAD"
    fi
    git -C "$DEST" checkout --detach "$COMMIT"
fi

if git -C "$DEST" apply --check --reverse "$PATCH" 2>/dev/null; then
    log "补丁已应用，跳过"
else
    git -C "$DEST" apply --check "$PATCH" \
        || fail "补丁无法干净应用（上游 revision 或补丁已变化）"
    git -C "$DEST" apply --whitespace=nowarn "$PATCH"
    log "补丁已应用"
fi

# ---- 叠加 iOS 平台定义 ----
mkdir -p "$DEST/pcsl/makefiles/platforms" \
         "$DEST/pcsl/types/darwin_ios_arm64" \
         "$DEPS/ios-overlay/include"
cp "$OVERLAY/pcsl/platforms/darwin_ios_arm64.gmk" \
   "$DEST/pcsl/makefiles/platforms/darwin_ios_arm64.gmk"
cp "$OVERLAY/pcsl/types/darwin_ios_arm64/java_types_md.h" \
   "$DEST/pcsl/types/darwin_ios_arm64/java_types_md.h"
cp -R "$OVERLAY/include/." "$DEPS/ios-overlay/include/"
cp "$OVERLAY/phoneme_ios_bridge.h" "$DEPS/ios-overlay/include/"

log "完成：$COMMIT + BBK9588 补丁 + darwin_ios_arm64 平台定义"
log "源码树：$DEST"
