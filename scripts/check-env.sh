#!/usr/bin/env bash
# 打印构建环境，便于在 Actions 日志里定位工具链差异
set -euo pipefail
echo "=== runner ==="
uname -a
echo "=== Xcode ==="
xcodebuild -version 2>/dev/null || true
xcode-select -p
echo "=== SDKs ==="
xcodebuild -showsdks 2>/dev/null | grep -i iphoneos || true
echo "=== clang ==="
xcrun --sdk iphoneos clang --version 2>/dev/null | head -2 || true
echo "=== tools ==="
for tool in xcodegen git python3 zip security codesign; do
    printf '%-12s %s\n' "$tool" "$(command -v "$tool" || echo missing)"
done
echo "=== JDK (romize 阶段需要) ==="
/usr/libexec/java_home -V 2>&1 | head -4 || true
echo "=== 资源校验 ==="
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
for f in ios/Resources/unifont-cjk-14x14.bin ios/Resources/phoneme-skin.bin \
         ios/Resources/j2me-coffee.png vendor/phoneme-feature-mr2-b23-bbk9588.patch; do
    printf '%-60s %s bytes\n' "$f" "$(wc -c < "$ROOT/$f" 2>/dev/null || echo MISSING)"
done
