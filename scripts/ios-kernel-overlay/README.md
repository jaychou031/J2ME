# phoneME → arm64-apple-ios 内核 overlay

这个目录是**从源码重编译 phoneME 内核**时，叠加到上游 `magicus/phoneME` 固定 commit
上的 iOS 平台适配层。它由 `scripts/build-phoneme-ios.sh` 在 GitHub Actions 的
macOS runner 上复制进 `.deps/phoneme/`，与上游源码一起编译成 `libphoneme_ios.a`。

## 为什么需要它

原仓库的 `third_party/phoneme/prebuilt/*.a` 是 **MIPS32 ELF 可重定位对象**，
其中 `libobj.a` 里的 `ROMImage.o` 是 romize 后的 Java 类库，内部指针是 32 位 MIPS
重定位。iOS 是 arm64、Mach-O，ABI 完全不兼容 —— **不能复用，必须重新 romize 并
重新编译全部 C/C++ 源码**。这也是本次移植工作量最大的部分。

## 文件对照

| overlay 文件 | 替换/对应的原项目文件 | 说明 |
| --- | --- | --- |
| `phoneme_ios_bridge.{h,c}` | `src/runtime/startup.c` + `phoneme_app.c` | 内核 ABI、生命周期、音量/键盘、诊断 |
| `ios_fb_port.c` | `phoneme_bbk9588_port.c` 末尾 fb_port 段 | Java screen buffer → `ios_fb_buffer()` |
| `bda_compat.c` + `include/bda_*.h` | `bda_audio` / `bda_gui` 固件服务 | 转发到 AudioQueue / 单调时钟 |
| `ios_bbk_compat.c` | `phoneme_bbk9588_port.c` 的时钟函数 | `j2me_phoneme_bbk9588_clock_ms/sleep_ms` |
| `include/platform/bbk9588/*` | 原项目同名头（原样复制） | 让 `phoneme_audio.c` 不改一行即可编译 |
| `pcsl/platforms/darwin_ios_arm64.gmk` | `linux_mips_gcc.gmk` | 64 位 / iOS SDK 编译参数 |
| `pcsl/types/darwin_ios_arm64/java_types_md.h` | `linux_mips_gcc/java_types_md.h` | arm64 类型定义 |
| 生成物 `cjk_font_data.c` | objcopy 嵌入的 `.rodata` | 定义 `_binary_phoneme_cjk_font_bin_start/end`，让原 `phoneme_cjk_text.c` 原样编译 |

## 尽量「原汁原味」复用的上游源码

构建脚本会把下面这些**原项目文件原样编译**（不做逻辑修改）：

- `vendor/phoneme_audio.c.orig` —— 完整 MMAPI：SMF 0/1 MIDI、PCM WAV、`playTone`、
  24 声部波形合成、0–100% 主音量。只需 7 个 `bda_audio_*` shim。
- `vendor/phoneme_cjk_text.c.orig` —— 14×14 CJK 二分查找与可变宽度绘制。
  只需由生成的 `cjk_font_data.c` 提供字库符号。
- `vendor/native_events.c.orig` —— 32 深事件队列、move 合并、关键事件保序。

## ⚠️ 首次编译必须对齐的地方

以下三点是上游构建系统的历史包袱，**无法在 Linux 沙箱里预先验证**，需要在
GitHub Actions 的 macOS runner 上按 `build/kernel/BUILD_FAILURE.md` 的日志迭代：

1. **ROMImage 的 64 位 romize**。phoneME Feature MR2 的 romizer 是 2008 年的 Java
   工具，依赖老式 JDK 与 32-bit GNU make。必须让它在 64 位配置下重新生成
   `ROMImage.cpp` / `jvmconfig.h` / `ROMStructs.h`；直接沿用 MIPS 版生成物会
   因指针宽度不同而崩溃。
2. **master mode 线程化**。原项目 `bda_main()` 同步调用 `runMidlet()`，因为
   9588 是裸机单线程。iOS 上 `runMidlet` 会阻塞主线程导致 UI 冻结，需要改为
   专用线程 + 非阻塞事件泵（`phoneme_ios_pump()` 里的 TODO）。
3. **输入事件投递符号**。`phoneme_ios_pump()` 里把 `g_j2me_events` 投递到 MIDP
   eventqueue 的具体函数名（`midp_store_*` 等）取决于 romize 结果，需按链接
   错误对齐。

另注意：上游补丁的 `midp/src/events/input_port/bbk9588/lib.gmk` 引用了
`phoneme_input_port.c`，但原仓库**并未提交该文件**（原构建用的是预构建对象，
这些 `.gmk` 只是为「可重放补丁」而保留）。因此 iOS 构建不能假设上游 make
能直接跑通，需要显式补齐输入端口。

## 迭代方式

```bash
# 在 macOS runner 上（或本地有 Xcode 时）
./scripts/bootstrap-phoneme.sh
./scripts/build-phoneme-ios.sh 2>&1 | tee build.log
cat build/kernel/BUILD_FAILURE.md      # 失败时自动生成，含完整环境信息
```

脚本对每个阶段都打印 `[stage N/6]` 并把 clang 的原始错误原样落盘，
补上缺失的 `-I` / 源文件 / 符号后重跑即可。CI 对这一步设置了
`continue-on-error`：即使内核尚未跑通，仍会产出可安装的诊断内核 IPA，
让你先在 iPhone 13 上验证缩放、触控、软键盘和 JAR 导入链路。
