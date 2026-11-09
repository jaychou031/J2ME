# 移植设计：BBK 9588 (MIPS 裸机) → iOS (arm64)

本文说明每个原项目模块在 iOS 上如何落地，以及为什么这样落地。
对照阅读 `vendor/README.upstream.md` 与 `vendor/architecture.upstream.md`。

## 1. 两个平台的根本差异

| | BBK 9588 / BDA | iPhone 13 / iOS 15 |
| --- | --- | --- |
| CPU / 目标格式 | MIPS32 小端 ELF，freestanding | arm64 Mach-O，dyld |
| 操作系统 | 无（裸机，自带 freestanding libc） | Darwin + libSystem + UIKit |
| 显示 | 固件物理 framebuffer 240×320 RGB565 | `CALayer.contents` + 缩放 |
| 输入 | `bda_gui_raw_event_fetch()` 原始事件、六键包 | `UIKit` 触摸 / `UIPress` |
| 文件 | `A:\` / `B:\` 固件文件系统，GBK 路径 | POSIX 沙箱，UTF-8 路径 |
| 音频 | `bda_audio` PCM 设备 | AudioToolbox `AudioQueue` |
| 内存 | 有界分配器 6 MiB 硬上限，Java heap 3 MiB | `malloc`，仍保留原配置值 |
| 资源嵌入 | `objcopy -I binary` 生成 `.o` 链进 ELF | App bundle resource |
| 启动 | `entry.S` → `bda_main()` → `runMidlet()` 同步跑 | `UIApplicationMain` → `CADisplayLink` 驱动 `pump()` |

**核心判断**：`third_party/phoneme/prebuilt/*.a` 是 MIPS 目标码，不能链接到
arm64 Mach-O。ROMImage 里的指针是 32 位 MIPS 重定位，在 64 位进程里必然崩溃。
所以 iOS 版必须**从上游源码重新 romize + 重新编译**，这是整个移植的主要工作量。

## 2. 模块逐项映射

| 原项目文件 / 模块 | iOS 对应 | 处理方式 |
| --- | --- | --- |
| `src/runtime/entry.S`, `startup.c` | `ios/App/main.m`, `AppDelegate.m` | 重写 |
| `src/platform/bbk9588/libc/freestanding_libc.c` | libSystem | 删除（不需要） |
| `phoneme_bbk9588_port.c` · framebuffer 段 | `ios/Platform/ios_framebuffer.c`, `ios_fb_port.c` | 重写，保留 240×320 RGB565 |
| `phoneme_bbk9588_port.c` · 软键盘/悬浮点 | `ios/Platform/ios_softpad.c` | **算法与常量原样复刻** |
| `phoneme_bbk9588_port.c` · 时钟 | `ios/Platform/ios_time.c` | `mach_absolute_time` / `clock_gettime` |
| `native_events.c/.h` | `ios/Platform/ios_events.c` | **语义原样移植**（32 深、move 合并、保序） |
| `phoneme_cjk_text.c` | `ios/Platform/ios_cjk_text.c`（App）+ `vendor/phoneme_cjk_text.c.orig`（内核） | **算法与字库原样** |
| `phoneme_audio.c` (MMAPI) | `vendor/phoneme_audio.c.orig` + `bda_compat.c` | **源码原样编译**，只换 7 个 shim |
| `phoneme_pcsl_file.c` (GBK/JAR cache/ZIP 规范化) | `ios/Platform/ios_jar.c` | ZIP 规范化算法逐行移植，文件后端换 POSIX |
| `phoneme_app.c` (启动器) | `ios/App/J2MEHostViewController.m` + `ios_jar.c` | 重写 UI，保留 Manifest 发现逻辑 |
| `phoneme_mastermode_port.c` | `ios/Kernel/*/pump()` + `CADisplayLink` | 重写事件泵 |
| `assets/runtime/unifont-cjk-14x14.bin` | `ios/Resources/`（App）/ `cjk_font_data.c`（内核） | **同一份字节** |
| `assets/runtime/phoneme-skin.bin` | `ios/Resources/` | 原样打包 |
| `linker/phoneme-bda.ld`, `pack_bda.py` | `scripts/package-ipa.sh` | 重写为 IPA 打包 |
| `tools/*.ps1` (Windows 构建) | `scripts/*.sh` + `.github/workflows/*.yml` | 重写为 macOS runner |

## 3. 保持「原汁原味」的关键设计

### 3.1 逻辑分辨率钉死在 240×320

所有 C 层代码（软键盘命中测试、CJK 绘制、脏区计算、事件队列）都使用
240×320 逻辑坐标。缩放只发生在 `J2MEScreenView` 的最后一步：

```
phoneME / 诊断内核
   ↓ 写 240×320 RGB565 (uint16_t[76800])
ios_fb_present()
   ↓ 单遍合成软键盘浮层（alpha 224/256 底板、208/256 按键）
onFrame 回调 → J2MEBridge → J2MEScreenView
   ↓ RGB565 → RGBA8888 → CGImage → CALayer.contents
   ↓ 整数倍最近邻 / 等比 / 拉伸
iPhone 13 屏幕（1170×2532 px）
```

触摸反向变换：`(location - screenRect.origin) / scale` → 逻辑坐标。
因此 `ios_softpad_handle_touch()` 收到的坐标和 9588 真机一致。

### 3.2 软键盘不是 UIKit 控件

原项目在 Java screen buffer 之上做单遍像素合成。iOS 版照搬：
几何常量、5×7 标签字形、alpha 系数、绿色音量条颜色、外圈白内圈灰的
悬浮点，全部一致。好处是放大后画面观感与真机一致；代价是按钮也随缩放
变大（在 iPhone 13 上 4× 时反而更好点）。

### 3.3 CJK 字库零改动

`unifont-cjk-14x14.bin` 格式：

```
[0..3]  "CJ14"
[4..5]  字形数 u16le = 28400
[6]     宽 14   [7] 高 14   [8] 每字形 25 字节   [9..11] 保留
[12..]  记录 = codepoint u16le + 25 字节 1-bit 位图（MSB first，行优先）
```

记录按 codepoint 升序，二分查找。App 层从 bundle 读入；内核层由
`build-phoneme-ios.sh` 生成 C 数组提供 `_binary_phoneme_cjk_font_bin_start/end`，
让原 `phoneme_cjk_text.c` 不改绘制算法即可编译。西文字库不含 ASCII，
因此另生成 8×11 字形表但保留 **9px advance / 14px 行高**，与原项目混排宽度一致。

### 3.4 老式 ZIP data descriptor 规范化

`ios_jar_normalize()` 是 `phoneme_pcsl_file.c` 同名函数的逐行移植：
只在 RAM 缓存里补齐 local header 的 size 字段、移除 16-byte descriptor、
修正 central directory offset。**不修改任何 class / resource 字节**。
这解决老式打包工具生成的 JAR 在 phoneME ZIP reader 里读不到的问题。

### 3.5 音频规格

保持 22050 Hz / 16-bit / mono、1024-byte 非阻塞块、0–100% 数字主音量。
原项目 `phoneme_audio.c` 里的 MIDI SMF 0/1 解析、24 声部波形合成、
PCM WAV、`playTone` 全部原样编译，只把 `bda_audio_write()` 转发到
`ios_audio_write()` → AudioQueue 环形缓冲（2 秒）。

## 4. iOS 侧新增的部分（原项目没有）

- **缩放与插值**：像素完美整数倍 / 等比 / 拉伸；最近邻 / 双线性。
- **横屏沉浸式全屏**、安全区适配、状态栏控制。
- **原生工具栏与设置页**（UIKit，适配高分屏），游戏区内软键盘保持原样。
- **JAR 库与导入**：`UIDocumentPicker`、文件 App 分享、iTunes 文件共享，
  对应原项目 `B:\J2ME\` / `A:\J2ME\` 的选择器目录。
- **外接键盘 / 手柄**映射。
- **截图分享**、运行诊断页（对应 `J2MEPHONE.LOG` 的计数器）。
- **诊断内核**：无 Mac 也能先验证 UI / 触控 / 缩放 / CJK / JAR 解析链路。

## 5. 已知不做的事

与原项目一致，**不支持** MP3 / AAC / AMR、录音、摄像头、蓝牙、
M3G (JSR-184)、厂商私有 API、网络接口。iOS 版额外不做后台运行、
Game Center、iCloud 同步。

## 6. 未验证边界

本工程在 Linux 沙箱里完成了 C 层与渲染链路的端到端验证，但
**Xcode / iOS SDK 编译与 phoneME 全量重编译只能在 GitHub Actions 的
macOS runner 上验证**。详情见 `docs/KERNEL_STATUS.md`。
