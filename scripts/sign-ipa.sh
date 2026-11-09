#!/usr/bin/env bash
# 可选：用 GitHub Actions Secrets 里的 p12 + mobileprovision 给 IPA 签名。
#
# 需要的 secrets（在仓库 Settings → Secrets and variables → Actions 里配置）：
#   P12_BASE64              开发/分发证书 .p12 的 base64
#   P12_PASSWORD            .p12 密码
#   MOBILEPROVISION_BASE64  .mobileprovision 的 base64
# 可选：
#   CODESIGN_IDENTITY      例如 "Apple Development: name (TEAMID)"
#   BUNDLE_ID              覆盖 Info.plist 的 CFBundleIdentifier（需与描述文件匹配）
#
# 不配置这些也能编译：CI 默认只产出未签名 IPA，用 TrollStore 或手机端
# 签名工具安装。见 docs/INSTALL.md。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
APP="$BUILD/BBK9588J2ME.app"
UNSIGNED="${1:-$BUILD/J2ME-unsigned.ipa}"
SIGNED="${2:-$BUILD/J2ME-signed.ipa}"

: "${P12_BASE64:?未设置 P12_BASE64}"
: "${P12_PASSWORD:?未设置 P12_PASSWORD}"
: "${MOBILEPROVISION_BASE64:?未设置 MOBILEPROVISION_BASE64}"

[ -d "$APP" ] || { echo "[sign] 缺少 $APP" >&2; exit 1; }

TMP="$BUILD/sign-tmp"
KEYCHAIN="$BUILD/j2me-sign.keychain-db"
rm -rf "$TMP"; mkdir -p "$TMP"
rm -f "$KEYCHAIN"

cleanup() {
    security delete-keychain "$KEYCHAIN" >/dev/null 2>&1 || true
    rm -rf "$TMP"
}
trap cleanup EXIT

echo "$P12_BASE64" | base64 --decode > "$TMP/cert.p12"
echo "$MOBILEPROVISION_BASE64" | base64 --decode > "$TMP/profile.mobileprovision"

security create-keychain -p "" "$KEYCHAIN"
security set-keychain-settings -lut 3600 "$KEYCHAIN"
security unlock-keychain -p "" "$KEYCHAIN"
security import "$TMP/cert.p12" -k "$KEYCHAIN" -P "$P12_PASSWORD" \
    -T /usr/bin/codesign -T /usr/bin/security
security set-key-partition-list -S apple-tool:,apple:,codesign: \
    -s -k "" "$KEYCHAIN" >/dev/null 2>&1 || true
security list-keychains -d user -s "$KEYCHAIN" login.keychain-db

IDENTITY="${CODESIGN_IDENTITY:-}"
if [ -z "$IDENTITY" ]; then
    IDENTITY="$(security find-identity -v -p codesigning "$KEYCHAIN" \
        | head -1 | sed -n 's/.*"\(.*\)"/\1/p')"
fi
[ -n "$IDENTITY" ] || { echo "[sign] 找不到可用的签名身份" >&2; exit 1; }
echo "[sign] 身份：$IDENTITY"

cp "$TMP/profile.mobileprovision" "$APP/embedded.mobileprovision"
security cms -D -i "$TMP/profile.mobileprovision" > "$TMP/profile.plist"
/usr/libexec/PlistBuddy -x -c 'Print :Entitlements' "$TMP/profile.plist" \
    > "$TMP/entitlements.plist"

if [ -n "${BUNDLE_ID:-}" ]; then
    /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $BUNDLE_ID" "$APP/Info.plist"
fi

codesign --force --sign "$IDENTITY" \
    --keychain "$KEYCHAIN" \
    --entitlements "$TMP/entitlements.plist" \
    --timestamp none \
    --generate-entitlement-der \
    "$APP"
codesign --verify --deep --strict "$APP"
echo "[sign] 签名完成"

STAGE="$BUILD/ipa-signed-stage"
rm -rf "$STAGE"; mkdir -p "$STAGE/Payload"
cp -R "$APP" "$STAGE/Payload/"
find "$STAGE" -name '.DS_Store' -delete
( cd "$STAGE" && zip -qry "$SIGNED" Payload )
rm -rf "$STAGE"
echo "[sign] 已签名 IPA：$SIGNED ($(wc -c < "$SIGNED") bytes)"
