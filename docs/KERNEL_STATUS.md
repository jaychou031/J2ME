# 内核状态与验证边界（请务必先读）

这份文档把「已经验证过的」和「还需要在 GitHub Actions / 真机上验证的」
分清楚。我不会把没跑通的东西写成已完成。

## 1. 已经在本地（Linux）验证通过的部分

| 项目 | 验证方式 | 结果 |
| --- | --- | --- |
| 全部平台层 C（7 个文件）+ Mock 内核 | `gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only` | 全部 PASS |
| 端到端渲染 | 编译成可执行文件，真实加载 `unifont-cjk-14x14.bin`，输出 240×320 RGB565 帧并转 PNG 目检 | 中文清晰、悬浮点/音量条/软键盘布局与原项目一致 |
| CJK 字库 | 解析 `CJ14` 头、二分查找 | 28,400 字形；`中文`=28px、`ABC`=27px，与原项目 14/9 advance 一致 |
| 事件队列 | 灌入 40 条 DRAG + 1 条 UP | `dropped=0 / coalesced=39 / peak=2`，UP 事件保序存活 |
| 软键盘 | 短按悬浮点、数字键 `5` 命中 | toggle 与 key event 计数正确 |
| JAR / Manifest | 构造标准 JAR 与带 16-byte data descriptor 的老式 JAR | 标准 JAR 解析出混淆类名 `b` 与中文名「帝国时代 III」；老式 JAR `normalized=1`，规范化后的字节能被 Python `zipfile` 正确读取且 CRC 匹配 |
| 配置 | `plistlib` 解析 `Info.plist`、`J2ME.entitlements`；PyYAML 解析 `project.yml` 与两个 workflow | 全部合法 |
| Objective-C 结构 | `@interface` / `@implementation` / `@end` 与括号配对检查 | 8 个 `.m` 全部平衡 |
| 补丁完整性 | SHA-256 对比 `vendor/deps.lock.psd1` | `8885E57A…B40462CC` 完全一致 |
| Shell 脚本 | `bash -n` | 见 CI 首次运行 |

## 2. 无法在这里验证的部分（原因）

沙箱是 **Ubuntu + gcc/clang（Linux）**，没有：

- macOS / Xcode / iPhoneOS SDK → 无法编译 UIKit / AudioQueue 代码、无法生成 `.app`/`.ipa`
- Apple 签名证书 → 无法验证签名与真机安装
- iOS 设备 → 无法真机验收

因此 **ObjC 代码、XcodeGen 工程、workflow、签名脚本都是按 iOS 15 API 写好的，
但尚未经过一次真实 `xcodebuild`**。第一次 push 触发 Actions 时，若出现
编译错误，通常是头文件路径、API 可用性或 XcodeGen 版本差异，
修起来一般是几行配置，不涉及架构调整。

## 3. phoneME 内核：已完成 vs 待迭代

### 已完成（可直接审阅）

- `scripts/bootstrap-phoneme.sh`：固定 commit、补丁 SHA-256 校验、应用补丁、
  叠加 `darwin_ios_arm64` 平台定义 —— 与原项目 `bootstrap.ps1` 语义一致。
- `scripts/ios-kernel-overlay/`：完整的 iOS 平台适配层：
  - `ios_fb_port.c` —— Java screen buffer → `ios_fb_buffer()`
  - `bda_compat.c` + `include/bda_*.h` —— 让原项目 26 KB 的 MMAPI 源码
    （MIDI/WAV/tone）**几乎不改一行**就能编译
  - `ios_bbk_compat.c` —— 补丁需要的时钟/诊断符号
  - `pcsl/` —— arm64 平台 gmk 与 64 位类型定义
  - `phoneme_ios_bridge.c` —— 生命周期、事件、音量、诊断
- `scripts/build-phoneme-ios.sh`：六阶段编译、CJK 数据生成、静态库打包、
  `nm` 符号校验、失败自动写 `BUILD_FAILURE.md`。
- `ios/Kernel/PhoneME/j2me_phoneme_kernel.c`：ABI 转发层。
- **原样复用**的说明：`vendor/phoneme_audio.c.orig`、
  `vendor/phoneme_cjk_text.c.orig`、`vendor/native_events.c.orig`。

### 待在 macOS runner 上迭代（三个真实难点）

1. **64 位 ROMImage 重新 romize**
   phoneME Feature MR2 的 romizer 是 2008 年的 Java 工具，依赖老式 JDK 与
   32-bit GNU make。原仓库的 `ROMImage.o` 是 MIPS 32 位，指针宽度不同。
   必须让 romizer 在 64 位配置下重新生成 `ROMImage.cpp` / `jvmconfig.h` /
   `ROMStructs.h`。这是整个移植最大的不确定点。

2. **master mode 线程化**
   原项目 `bda_main()` 在裸机单线程里同步调用 `runMidlet()`。iOS 上这会
   阻塞主线程导致 UI 冻结，需要改成「专用线程跑 KVM + 非阻塞事件泵」。
   `phoneme_ios_start_jar()` / `phoneme_ios_pump()` 里已标出改造点。

3. **MIDP 事件投递符号**
   把 `g_j2me_events` 投递到 MIDP eventqueue 的具体函数名
   （`midp_store_*` 等）取决于 romize 结果，需要按链接错误对齐。

另有一个上游自身的坑：补丁里
`midp/src/events/input_port/bbk9588/lib.gmk` 引用了 `phoneme_input_port.c`，
但**原仓库并没有提交这个文件**（原构建用预构建对象，这些 `.gmk` 只为
「可重放补丁」保留）。所以不能假设上游 make 能直接跑通，需要补一个
iOS 输入端口。

## 4. 为什么这样交付

你有 iPhone 但没有 Mac，如果只给一个「理论上能编译」的工程，
push 之后 CI 失败就拿不到任何可安装的东西。所以工程被设计成：

- **默认 `mock` 内核**：CI 第一次运行就能产出可安装的 IPA，
  让你先在真机上验证缩放、悬浮点/软键盘、触控、CJK、JAR 导入、音频链路。
- **phoneME 内核是可选的第二阶段**：`workflow_dispatch` 里选 `phoneme`，
  `continue-on-error` 保证失败也不影响出包，并把完整日志和
  `BUILD_FAILURE.md` 上传成 artifact。
- **两种内核共用 `j2me_kernel.h` ABI**：内核接通后 App 层一行都不用改，
  只需 `KERNEL_BACKEND=phoneme`。

## 5. 建议的推进顺序

1. Fork → push → 用默认 `mock` 跑一次 Actions，确认 IPA 能编译、能装、能显示。
   如果这一步报错，把 `xcodebuild` 日志贴出来，通常是工程配置级别的小问题。
2. 在 iPhone 13 上确认画面、缩放、软键盘、导入 JAR 都正常。
3. 用 `kernel_backend=phoneme` 跑一次，下载
   `phoneme-kernel-build-failure` artifact，按 `BUILD_FAILURE.md` 和
   `compile.log` / `make.log` / `symbols.txt` 补齐 64 位 romize 与缺失符号。
4. 内核跑通后，把 `ios.yml` 的默认后端改为 `phoneme`，即可真正执行 Java MIDlet。

## 6. 我不会替你声称的事

- 不会说「已成功编译出 iOS App」——本环境没有 Xcode。
- 不会说「phoneME 内核已跑通 jar 游戏」——romize 与线程化尚未在 macOS 上验证。
- 不会说「已通过 App Store 审核」——本工程走侧载，不需要也不适合上架。

这些结论只能由你在 GitHub Actions 与真机上得到。
