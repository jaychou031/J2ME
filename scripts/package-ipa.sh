#!/usr/bin/env bash
# 把未签名 .app 打成标准 IPA（不需要 Mac 上的签名证书）
# 未签名 IPA 可直接用 TrollStore 安装，或交给手机端签名工具 / sign-ipa.sh 签名。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
APP="$BUILD/BBK9588J2ME.app"
OUT="${1:-$BUILD/J2ME-unsigned.ipa}"
# $OUT 可能是相对路径（CI 传的是 build/J2ME-unsigned.ipa）。下面会 cd 进
# STAGE 再调 zip，相对路径会被解析成 $STAGE/build/... —— 那个目录不存在，
# zip 会报 "Could not create output file"。统一转成基于 ROOT 的绝对路径。
case "$OUT" in
    /*) ;;
    *) OUT="$ROOT/$OUT" ;;
esac
mkdir -p "$(dirname "$OUT")"
rm -f "$OUT"

[ -d "$APP" ] || { echo "[ipa] 缺少 $APP，请先运行 scripts/build-app.sh" >&2; exit 1; }

STAGE="$BUILD/ipa-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/Payload"
cp -R "$APP" "$STAGE/Payload/"
find "$STAGE" -name '.DS_Store' -delete

# 未签名构建不应带 _CodeSignature；若存在会影响侧载工具
rm -rf "$STAGE/Payload/BBK9588J2ME.app/_CodeSignature"

( cd "$STAGE" && zip -qry "$OUT" Payload )
rm -rf "$STAGE"

echo "[ipa] 未签名 IPA：$OUT ($(wc -c < "$OUT") bytes)"
echo "[ipa] 校验结构："
unzip -l "$OUT" | head -12 || true
