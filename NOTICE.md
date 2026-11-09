# 第三方归属声明（NOTICE）

## 上游项目

- **BBK 9588 J2ME** — HelloClyde，2026，GPL-2.0-only
  <https://github.com/HelloClyde/BBK9588-J2ME>
  本工程移植自该项目的架构、算法与资源。上游原始 README、架构说明与
  NOTICE 保留在 `vendor/README.upstream.md`、`vendor/architecture.upstream.md`、
  `vendor/NOTICE.upstream.md`。

- **phoneME Feature MR2** — 版权归 Sun Microsystems, Inc. 及其贡献者所有，
  原文件按 **GNU GPL version 2 only** 发布。
  固定使用 `magicus/phoneME` 的
  `149588a05f20d1b0147fda6bfe1ca27844b9fff7`。
  BBK 9588 移植改动以补丁形式提供（`vendor/phoneme-feature-mr2-b23-bbk9588.patch`）。

## 字体

- **GNU Unifont 15.1.05** — 版权、作者列表与 SIL Open Font License 1.1 全文见
  `vendor/GNU-Unifont-OFL-1.1.txt`。
  `ios/Resources/unifont-cjk-14x14.bin` 是从 `unifont-16.bdf` 生成的
  Modified Version（28,400 个 14×14 字形）。

## iOS 移植部分

- iOS App / 平台适配层 / 构建脚本 / 文档为本移植工程原创，按 GPL-2.0-only 发布。
- App 图标由上游 `assets/icons/j2me-coffee.png` 放大生成。

## 商标

`Java`、`BBK` 及游戏名称与商标归各自权利人所有。本工程是非官方社区项目，
不隶属于或代表步步高、Sun Microsystems、Oracle、EA、Glu Mobile 或其他厂商。
仓库与 Release 不包含商业游戏 JAR、固件或 NAND 镜像。
