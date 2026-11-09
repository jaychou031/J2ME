# BBK 9588 J2ME

[![发布构建](https://github.com/HelloClyde/bbk9588-J2ME/actions/workflows/release.yml/badge.svg)](https://github.com/HelloClyde/bbk9588-J2ME/actions/workflows/release.yml)
[![GitHub Release](https://img.shields.io/github/v/release/HelloClyde/bbk9588-J2ME?display_name=tag)](https://github.com/HelloClyde/bbk9588-J2ME/releases)
[![License: GPL-2.0](https://img.shields.io/badge/license-GPL--2.0--only-blue.svg)](LICENSE)

面向 BBK 9588（C200 / kj409588）的 J2ME 运行时，基于 phoneME Feature MR2 / KVM，
提供 CLDC 1.1、MIDP 2.0、LCDUI、GameCanvas、RMS、中文点阵和轻量 MMAPI 支持。

项目目标不是在桌面上模拟一台手机，而是把 J2ME 运行时直接做成 9588 的原生 BDA 应用：
Java 画面以 RGB565 写入固件 framebuffer，触摸和实体键读取固件原始输入，动态内存硬限制为
6 MiB。

## 运行预览

![BBK 9588 J2ME 运行预览](assets/screenshots/preview.png)

上图是 240×320 framebuffer 的真实运行截图，由 bbk9588-emulator v0.1.5 抓取；游戏
JAR、素材和 ROM 不包含在本仓库或 Release 中。截图中的游戏仅用于兼容性展示，相关权利
归原权利人所有。

## 功能

- 从 9588 原生文件选择器打开任意 JAR：优先 `B:\J2ME\`，没有 B: 时使用 `A:\J2ME\`。
- 从 `META-INF/MANIFEST.MF` 自动解析 `MIDlet-1`，兼容混淆后的 MIDlet 类名。
- 240×320 RGB565 直接输出，不依赖窗口触摸消息或周期性 GUI Timer。
- 实体方向、确认、返回键映射为 MIDP game action。
- 可拖动悬浮点和深色半透明软键盘，补充 `L`、`R`、数字、`*`、`#`。
- 悬浮菜单支持退出确认、静音和 0–100% 主音量。
- 内置 28,400 个 14×14 CJK 字形，覆盖常用中文、标点和全角字符。
- RMS 持久化；B: 存在数据时优先使用 B:，否则使用 A:。
- MMAPI 支持 MIDI（SMF 0/1）、PCM WAV 和 `Manager.playTone()`。
- 兼容部分老式 ZIP data descriptor JAR；规范化只发生在 RAM，不修改原文件。

## 快速开始

1. 从 [Releases](https://github.com/HelloClyde/bbk9588-J2ME/releases) 下载最新的
   `J2ME.bda`。
2. 按普通 9588 BDA 应用的方式把它安装到设备。
3. 把你合法持有的游戏 JAR 放进 `B:\J2ME\`；设备没有 B: 时放进 `A:\J2ME\`。
4. 启动 `J2ME`，在文件选择器中选择 JAR。

phoneME/KVM、MIDP 类库和中文字库都已嵌入 BDA，不需要额外放置任何系统 JAR 或 runtime。
游戏 JAR 和可写用户数据使用不同目录：

```text
B:\J2ME\                游戏 JAR 默认目录
A:\J2ME\                没有 B: 时的游戏 JAR 目录
B:\应用\数据\J2ME\    已有用户数据时优先，日志也在这里
A:\应用\数据\J2ME\    用户数据回退目录，日志也在这里
```

`应用\数据\J2ME` 里只有 RMS 存档、appdb 元数据、自动生成的 `skin.bin` 和
`J2MEPHONE.LOG`，不是运行时；删除它不会删掉 phoneME，但会清除相应游戏数据和日志。

### 悬浮键盘

- 短按右上角悬浮点：展开或收起软键盘。
- 拖动超过 12px：移动悬浮点；面板自动显示在另一半屏幕。
- `L` / `R`：MIDP 软键 `-6` / `-7`。
- `X`：调用 SDK 系统“是/否”确认框，选择“是”后安全销毁 MIDlet。
- `-` / `+`：每次调整 10% 主音量；`M`：静音。

## 兼容性

已经用真实《帝国时代 III Mobile》JAR 验证：中文菜单、剧情、教程、关卡载入、地图场景、
实体键、软键、RMS 和 MIDI 音乐。该结果只代表当前测试样本，不意味着所有厂商扩展 API
均受支持。

当前不支持 MP3、AAC、AMR、录音、摄像头、蓝牙、M3G/JSR-184 和厂商私有 API；网络
接口也没有作为兼容目标。连续拖动在 v0.1.5 模拟器中受 SADC 模型限制，建议在真机验收。

## 内存

《帝国时代 III Mobile》实际地图场景的账本：

| 项目 | 字节 | MiB |
| --- | ---: | ---: |
| BDA 静态映像（CJK、软键盘、MMAPI） | 2,104,524 | 2.01 |
| 动态分配峰值 | 3,940,393 | 3.76 |
| 物理 framebuffer | 153,600 | 0.15 |
| 已核算合计 | 6,198,517 | 5.91 |

Java heap 固定为 3 MiB，BDA 分配器上限为 6 MiB。最保守上界为 8.30 MiB，低于项目的
10 MiB 预算。详细设计和测试数据见 [架构说明](docs/architecture.md)。

## 从源码构建

建议使用 Windows 10/11、PowerShell 7、Git 和 Python 3。首次构建会下载固定版本的
phoneME 源码和 MIPS 交叉工具链，并验证 commit、补丁与预构建对象哈希。

```powershell
git clone --recursive https://github.com/HelloClyde/bbk9588-J2ME.git
cd bbk9588-J2ME
.\tools\bootstrap.ps1
.\tools\build-phoneme-bda.ps1 -AllocationLimitMb 6
```

输出文件：`build\phoneme\J2ME.bda`。

项目携带的 phoneME 预构建对象用于避开 Feature MR2 年代久远的完整 ROM 构建环境；CI 会
重新编译全部 9588 原生端口、嵌入资源、链接最终 ELF，并校验 BDA 头与校验和。对应的
phoneME commit、完整移植补丁和对象 SHA-256 均已入库，见
[third_party/phoneme/README.md](third_party/phoneme/README.md)。

重新生成 README 拼图：

```powershell
.\tools\build-preview.ps1
```

## Tag 发布

推送 `v*` tag 会触发 [release.yml](.github/workflows/release.yml)：

```powershell
git tag v0.1.0
git push origin v0.1.0
```

Workflow 会在 Windows runner 上完成 bootstrap 和正式构建，然后把 `J2ME.bda`、带许可证
的安装 ZIP、完整对应源码 ZIP 和 `SHA256SUMS.txt` 上传到同名 GitHub Release。

## 目录

```text
assets/                  图标、skin、CJK 点阵和 README 截图
docs/architecture.md     架构、内存账本和验证结果
linker/                  BDA 链接脚本
src/platform/bbk9588/    phoneME 的 9588 原生端口
src/runtime/             freestanding BDA 启动代码
third_party/phoneme/     固定源码补丁、预构建对象和哈希
tools/                   bootstrap、构建、打包和预览脚本
sdk/                     BBK 9588 BDA SDK 子模块
```

## 贡献与问题报告

请先阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。报告兼容问题时可附 JAR 的名称、厂商、
分辨率、Manifest 和 `J2MEPHONE.LOG`，但不要上传商业游戏 JAR、固件、NAND 镜像或个人
存档。

## 许可证与声明

本项目原创代码按 [GNU GPL v2 only](LICENSE) 发布。phoneME 保留 Sun Microsystems 的
GPL v2 only 文件头；GNU Unifont 子集按 SIL OFL 1.1 分发；BDA SDK 原创内容采用
`Apache-2.0 OR GPL-2.0-only` 双许可证，本项目选择 GPL-2.0-only。完整归属见
[NOTICE.md](NOTICE.md) 和各第三方目录。

这是非官方社区项目，与步步高、Java/Oracle、EA、Glu Mobile 或其他厂商无隶属、授权或
背书关系。Java、BBK 及游戏名称和商标归各自权利人所有。
