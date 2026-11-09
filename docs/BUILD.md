# 构建说明（GitHub Actions / 无 Mac）

## 1. Workflow 总览

`.github/workflows/ios.yml` 在 `macos-14` runner 上执行，触发条件：
push 到 `main/master`、PR、或手动 `workflow_dispatch`。

| 步骤 | 脚本 | 说明 |
| --- | --- | --- |
| 检查工具链 | `scripts/check-env.sh` | 打印 Xcode / SDK / clang / JDK / 资源大小 |
| 安装 XcodeGen | `brew install xcodegen` | 由 `ios/project.yml` 生成 `.xcodeproj`（不提交进仓库） |
| （可选）拉取 phoneME | `scripts/bootstrap-phoneme.sh` | 固定 commit + 校验补丁 SHA-256 + 应用补丁 + 叠加 iOS 平台定义 |
| （可选）编译内核 | `scripts/build-phoneme-ios.sh` | `continue-on-error`，失败自动降级 |
| 编译 App | `scripts/build-app.sh` | `xcodebuild -sdk iphoneos`，未签名 |
| 打包 IPA | `scripts/package-ipa.sh` | `Payload/*.app` → `J2ME-unsigned.ipa` |
| （可选）签名 | `scripts/sign-ipa.sh` | 用 Secrets 中的 p12 + 描述文件 |
| 上传 | `actions/upload-artifact@v4` | IPA + `SHA256SUMS.txt` |

`ios-release.yml` 在推 `v*` tag 时额外发布 Release，附带 IPA、校验和、
许可证文件和**完整对应源码 ZIP**（GPL-2.0 要求）。

### 手动运行参数

| 输入 | 默认 | 含义 |
| --- | --- | --- |
| `kernel_backend` | `mock` | `mock` = 诊断内核（稳定）；`phoneme` = 尝试从源码编译 phoneME |
| `sign` | `false` | 是否用 Secrets 签名 |

push / PR 触发时默认 `mock`，保证每次都有可安装的 IPA。

## 2. 关键 build settings

全部在 `ios/project.yml`，可用 `xcodebuild <KEY>=<VALUE>` 覆盖：

| 设置 | 值 | 说明 |
| --- | --- | --- |
| `IPHONEOS_DEPLOYMENT_TARGET` | `15.0` | 满足 iOS 15 兼容要求 |
| `TARGETED_DEVICE_FAMILY` | `1,2` | iPhone + iPad |
| `ARCHS` | `arm64` | iPhone 13；模拟器另设 `arm64 x86_64` |
| `ENABLE_BITCODE` | `NO` | Xcode 14+ 已废弃 bitcode |
| `CODE_SIGNING_ALLOWED` | `NO` | 产出未签名 `.app` |
| `J2ME_USE_PHONEME_KERNEL` | `0` | `0`=诊断内核，`1`=链接 `libphoneme_ios.a` |
| `J2ME_EXTRA_LDFLAGS` | 空 | 注入 `-lphoneme_ios` |
| `J2ME_EXTRA_LIB_SEARCH_PATHS` | 空 | 注入 `build/kernel/lib` |
| `J2ME_EXTRA_HEADER_SEARCH_PATHS` | 空 | 注入 `phoneme_ios_bridge.h` 所在目录 |

改用 phoneME 内核时，`build-app.sh` 会自动检测
`build/kernel/lib/libphoneme_ios.a`，存在则注入上面四项；
不存在（内核编译失败）则回退到 `mock`，**App 代码零改动**。

## 3. 本地/有 Mac 时的等价命令

```bash
# 0) 目录已是完整仓库根
brew install xcodegen

# 1) 诊断内核（一定会成功）
KERNEL_BACKEND=mock bash scripts/build-app.sh
bash scripts/package-ipa.sh

# 2) 尝试 phoneME 内核
bash scripts/bootstrap-phoneme.sh
bash scripts/build-phoneme-ios.sh
KERNEL_BACKEND=phoneme bash scripts/build-app.sh
```

产物：`build/BBK9588J2ME.app`、`build/J2ME-unsigned.ipa`、
（可选）`build/J2ME-signed.ipa`、`build/kernel/BUILD_FAILURE.md`。

## 4. phoneME 内核编译的六个阶段

`scripts/build-phoneme-ios.sh` 每阶段都打印 `[stage N/6]`，日志落盘在
`build/kernel/`：

| 阶段 | 输出 | 失败时的含义 |
| --- | --- | --- |
| 1/6 环境 | — | 没装 Xcode / iPhoneOS SDK |
| 2/6 生成 CJK 数据 | `obj/cjk_font_data.c`、`obj/cjk_text_patched.c` | 字库文件缺失或 magic 不对 |
| 3/6 编译 overlay | 8 个 `.o`，日志 `compile.log` | 缺头文件；看 `compile.log` 补 `-I` |
| 4/6 phoneME 核心 | 核心 `.a`，日志 `make.log` | **最可能失败**：64 位 romize / make 入口 / 缺失输入端口 |
| 5/6 打包 | `lib/libphoneme_ios.a` | `ar` 或核心库路径问题 |
| 6/6 符号校验 | — | `symbols.txt`，看缺哪个符号再补对应源文件 |

任一阶段失败都会生成 `build/kernel/BUILD_FAILURE.md`（含 runner / Xcode /
SDK / phoneME commit）并 `exit 1`。CI 把它作为
`phoneme-kernel-build-failure` artifact 上传。

### 常用覆盖变量

```bash
# 已有 arm64 核心库，跳过阶段 4
PHONEME_PREBUILT_LIB=/path/to/core.a bash scripts/build-phoneme-ios.sh

# 指定 phoneME make 入口与目标
PHONEME_MAKE_DIR=.deps/phoneme/midp/build/common \
PHONEME_MAKE_TARGET=all \
bash scripts/build-phoneme-ios.sh
```

## 5. 排错清单

| 症状 | 检查 |
| --- | --- |
| `xcodegen: command not found` | `brew install xcodegen`；或提交生成好的 `.xcodeproj` |
| `no such module UIKit` / SDK 错误 | `xcode-select -p` 是否指向完整 Xcode（不是 CommandLineTools） |
| `unsupported deployment target` | 用 Xcode 15.x（macos-14 runner）；iOS 15.0 SDK 仍可用 |
| 补丁 SHA-256 不匹配 | 不要改 `vendor/phoneme-feature-mr2-b23-bbk9588.patch`；lock 在 `vendor/deps.lock.psd1` |
| `romizer` / `ROMImage` 相关错误 | 见 `scripts/ios-kernel-overlay/README.md` 第 1 点：必须 64 位重新 romize |
| 大量 `undefined symbol: midp_*` | 阶段 4 没把 midp 对象合进静态库；用 `PHONEME_PREBUILT_LIB` 指定 |
| `missing phoneme_input_port.c` | 上游补丁引用了原仓库未提交的文件，需补一个 iOS 输入端口 |
| App 链接报 `undefined: _phoneme_ios_init` | `J2ME_EXTRA_LDFLAGS=-lphoneme_ios` 没注入；确认脚本检测到核心库 |
| App 链接报 `undefined: _runMidlet` | 核心库缺 `runMidlet`；阶段 6 会拦下并给出 `symbols.txt` |

## 6. 额度与仓库可见性

- **公开仓库**：GitHub 托管的 macOS runner 免费（推荐）。
- **私有仓库**：macOS 按 **10× 分钟**计费，免费额度 2000 分钟/月约合
  200 分钟实际构建时间。一次完整构建通常 5–15 分钟。
- `bootstrap-phoneme.sh` 会 clone 整个 phoneME 源码树（较大），
  可考虑在 workflow 里用 `actions/cache` 缓存 `.deps/phoneme`。

## 7. 改 Bundle ID / 版本号

- Bundle ID：`ios/project.yml` 的 `PRODUCT_BUNDLE_IDENTIFIER`
  （默认 `cn.bbkclyde.j2me.ios`）。若用签名，需与描述文件的 App ID 一致，
  或通过 Secrets 的 `BUNDLE_ID` 覆盖。
- 版本：`MARKETING_VERSION` / `CURRENT_PROJECT_VERSION`。
- 图标：`ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png`
  （由原项目 `assets/icons/j2me-coffee.png` 放大生成）。
