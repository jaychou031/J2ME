# 许可证与合规说明

## 1. 各组件许可

| 组件 | 许可 | 来源 |
| --- | --- | --- |
| 本工程原创代码（iOS App / 平台层 / 脚本 / 文档） | **GPL-2.0-only** | 与上游保持一致 |
| phoneME Feature MR2（CLDC/KVM/MIDP/PCSL） | **GPL-2.0-only**（Sun Microsystems 文件头） | `magicus/phoneME` @ `149588a0…` |
| BBK 9588 移植补丁 | GPL-2.0-only | `vendor/phoneme-feature-mr2-b23-bbk9588.patch` |
| 14×14 CJK 点阵字库 | **SIL Open Font License 1.1** | GNU Unifont 15.1.05 `unifont-16.bdf` 的修改版 |
| 西文 8×11 字形表 | 由 Pillow 内置位图字体提取生成 | `ios/Platform/ios_latin_font.h`（构建期生成） |
| App 图标 | 由原项目 `j2me-coffee.png` 放大生成 | `vendor/` |

完整许可文本：`LICENSE`（GPL-2.0）、
`vendor/GNU-Unifont-OFL-1.1.txt`（OFL 1.1）、
`vendor/NOTICE.upstream.md`（上游归属）。

## 2. GPL-2.0 的义务

- **源码对应**：分发二进制（IPA / 静态库）时必须同时提供完整对应源码。
  `ios-release.yml` 会自动把仓库源码打成 `J2ME-iOS-source.zip` 附在 Release 里。
- **同许可证**：衍生作品整体按 GPL-2.0-only 发布，不得改成更宽松的条款。
- **保留声明**：不得移除 phoneME 源文件的 Sun GPLv2 文件头与本工程的归属声明。
- **不使用 GPLv3 / 更晚版本**：上游明确是 `GPL v2 only`，注意 OFL 与
  Apache-2.0 组件不能混用不兼容条款。

> 原项目的 `sdk/` 是 `HelloClyde/bbk9588-bda-sdk` 子模块，采用
> `Apache-2.0 OR GPL-2.0-only` 双许可，原项目选择 GPL-2.0-only。
> **本 iOS 工程未包含该 SDK**（所有固件依赖都已换成 iOS 实现），
> 因此不涉及该子模块的再分发。

## 3. 字库（OFL 1.1）

`ios/Resources/unifont-cjk-14x14.bin` 是从 GNU Unifont 派生的
Modified Version，按 OFL 1.1 需保留字体名称、版权与许可声明；
`vendor/GNU-Unifont-OFL-1.1.txt` 已随仓库与 Release 分发。
生成脚本见原项目 `tools/build-cjk-font.py`（本仓库保留其产物，未改字节）。

## 4. 商标与内容

- 本工程是**非官方社区移植**，与步步高（BBK）、Oracle/Sun、EA、
  Glu Mobile 等无任何隶属、授权或背书关系。
- `Java`、`BBK`、游戏名称与商标归各自权利人所有。
- 仓库与 Release **不包含任何游戏 JAR、设备固件、NAND 镜像或个人存档**。
  请只导入你合法持有的游戏文件。

## 5. 侧载与 Apple 条款

- 本工程产出**未签名 IPA**，用于 TrollStore / 手机端签名工具等侧载途径；
  不走 App Store，也不含绕过 Apple 付费机制的内容。
- 使用个人/企业证书签名、TrollStore、DNS 规避吊销等做法，
  是否违反你所在地区的法规或 Apple 条款，由使用者自行判断并承担责任。
- 本工程不提供、不推荐任何**来路不明的泄露企业证书**；
  文档里明确建议使用你自己的 Apple Developer 证书。

## 6. 变更记录

- 从上游复制的源码与补丁放在 `vendor/`，文件名保留 `.orig` 后缀，
  内容**未改动**；构建期生成的 `cjk_text_patched.c` 只替换两行 extern
  声明（把 `end - start` 换成显式 size），绘制算法一个字节未改。
- 其余 iOS 适配代码为本工程原创，按 GPL-2.0-only 发布。
