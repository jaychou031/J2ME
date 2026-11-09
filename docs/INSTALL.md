# 没有 Mac，怎么把 IPA 装到 iPhone 13（iOS 15）

你只需要：**GitHub 账号 + iPhone 13 + 网络**。编译在 GitHub Actions 的
macOS runner 上完成，IPA 由 Actions 产出，安装全部在手机上操作。

## 0. 先拿到 IPA

- **方式 A（Artifact）**：仓库 → `Actions` → 运行 `iOS 构建（iPhone 13 / iOS 15+）`
  → 完成后下载 `J2ME-iOS-IPA`，解压得到 `J2ME-unsigned.ipa`。
  手机 Safari 登录 GitHub 也能下载（需登录）。
- **方式 B（Release，更方便手机直接下）**：推一个 tag，
  `git tag v0.1.0 && git push origin v0.1.0`，`ios-release.yml` 会把 IPA
  挂到 Release 附件上。公开 Release 的附件可以在 Safari 里直接下载到「文件」App。

> Artifact 默认保留 90 天；Release 是长期保存，推荐发布后从 Release 下载。

## 1. 选安装方式（按你的 iOS 版本）

| 方式 | 需要什么 | 有效期 | 是否需要电脑 | 适合 |
| --- | --- | --- | --- | --- |
| **TrollStore 2** | 设备已装 TrollStore | **永久** | 否 | iOS 15.x 首选，直接装未签名 IPA |
| **Feather / ESign 等手机端签名工具** | `.p12` + `.mobileprovision` 证书 | 视证书（个人 7 天 / 开发者 1 年） | 否 | 没有 TrollStore 时 |
| **GitHub Actions 直接签名** | 把证书放进仓库 Secrets | 同上 | 否 | 想让 CI 每次产出「装了就能用」的 IPA |
| AltStore / SideStore | AltServer 首次配对 | 7 天（SideStore 可在设备上刷新） | **首次需要** | 有别的电脑可借时 |

### 1.1 TrollStore 2（iOS 15 上最省事）

TrollStore 利用 CoreTrust 漏洞，安装的 App **永久有效、不需要证书**。
iPhone 13 是 arm64e（A15），iOS 15 全系基本都有对应安装途径：

| 你的 iOS 版本 | 常见安装 TrollStore 的途径 |
| --- | --- |
| 15.0 – 15.4.1 | TrollHelperOTA（Safari 在线装，**完全不需要电脑**） |
| 15.5 – 15.6.1 | TrollInstallerX / TrollInstallerMDC |
| 15.6 beta、15.7.x | TrollHelperOTA / TrollMisaka / TrollInstallerMDC（依版本） |

> 上表是社区常用对应关系，**具体以 TrollStore 官方项目当前的支持矩阵为准**；
> iOS 16.7.x（非 RC）与 17.0.1 及以上已修复该漏洞，无法使用。
> 你要求兼容 iOS 15，所以这条路径基本可用。

步骤：

1. 先按你的**确切 iOS 版本**装好 TrollStore 2（设置 → 通用 → 关于本机看版本号）。
2. 打开 TrollStore → `Settings` → **Install ldid**（第一次必须，否则装不了未签名 IPA）。
3. 建议再执行 `Install Persistence Helper`，选一个不常用的系统 App（如「提示」），
   避免重启后 App 打不开。
4. 在「文件」App 里找到 `J2ME-unsigned.ipa` → 分享面板 → **用 TrollStore 打开**
   → 点安装。完成，桌面出现 `J2ME` 图标。

### 1.2 手机端签名工具（Feather / ESign）

如果你的系统装不了 TrollStore，用**在手机上完成签名**的工具：

1. 装一个签名 App（**Feather** 是开源、使用标准 `.p12` + `.mobileprovision`；
   ESign 已停止维护，可优先选 Feather 或 KSign）。
2. 准备证书：
   - 最稳妥：自己的 Apple Developer 证书（个人账号 ¥688/年，或免费 Apple ID
     配合相应工具生成 development 证书），导出 `.p12` + `.mobileprovision`；
   - 社区免费/共享企业证书虽然可用，但**属于他人泄露的证书**，有隐私与
     随时被吊销的风险，不建议用于日常主力机。
3. 在签名 App 里 `Settings → Certificates → +` 导入 `.p12`（填密码）和
   `.mobileprovision`。
4. `Library → +` 导入 `J2ME-unsigned.ipa` → 选中 → `Sign` → `Install`。
5. 首次打开若提示「不受信任的开发者」：
   设置 → 通用 → **VPN 与设备管理** → 选中证书 → 信任。
6. iOS 16+ 还需在 设置 → 隐私与安全性 → **开发者模式** 打开；iOS 15 一般不需要。

> 免费个人证书签名的 App **7 天过期**，需要重新签名；开发者证书为 1 年。

### 1.3 让 GitHub Actions 直接产出已签名 IPA

如果你有 `.p12` + `.mobileprovision`，可以一次配置、以后每次构建自动签名：

1. 把两个文件转成 base64：
   ```bash
   base64 -i cert.p12 | pbcopy            # macOS
   # 或 base64 -w0 cert.p12                # Linux
   base64 -w0 profile.mobileprovision
   ```
2. 仓库 `Settings → Secrets and variables → Actions` 新建：
   `P12_BASE64`、`P12_PASSWORD`、`MOBILEPROVISION_BASE64`；
   可选 `CODESIGN_IDENTITY`（如 `Apple Development: 张三 (ABCDE12345)`）
   与 `BUNDLE_ID`（若描述文件的 App ID 不是默认的
   `cn.bbkclyde.j2me.ios`，必须覆盖成一致的）。
3. 运行 workflow 时把 `sign` 勾上；产物里会出现 `J2ME-signed.ipa`。

> 这一步只是**用已有证书签名**，证书仍需你自行取得；Actions 的 macOS runner
> 不会代替你登录 Apple 账号。不要把 Apple ID 密码放进仓库。

## 2. 装好之后：导入 JAR

对应原项目 `B:\J2ME\`（没有 B: 时 `A:\J2ME\`）的游戏目录，iOS 上是
App 的 `Documents/J2ME/`：

- **文件 App 分享**：在「文件」里长按 `.jar` → 共享 → 选 `J2ME`，App 会
  自动导入并启动（已声明自定义 UTI `cn.bbkclyde.j2me.jar`，也兼容 ZIP 识别）。
- **App 内导入**：底部「选游戏」→ 右上 `+` → 从「文件」选择（支持多选）。
- **iTunes / Finder 文件共享**：`UIFileSharingEnabled=YES`，可直接把 JAR
  拖进 App 的文档目录；进 App 点「选游戏」即可看到。
- 列表会显示文件大小和解析出的 `MIDlet-1` 主类名；左滑可删除。

> 请只导入你**合法持有**的游戏 JAR。本仓库不包含任何游戏、ROM 或固件。

## 3. 常见问题

| 现象 | 处理 |
| --- | --- |
| TrollStore 装 IPA 报 ldid 相关错误 | TrollStore → Settings → Install ldid 后重试 |
| 重启后 App 打不开 | TrollStore → Install Persistence Helper，并把它注入一个系统 App |
| 「不受信任的开发者」 | 设置 → 通用 → VPN 与设备管理 → 信任证书 |
| 「无法验证 App」/ 安装失败 | 关闭 VPN、广告拦截 DNS 后重试；或证书已被吊销需换证书 |
| 7 天后 App 打不开 | 免费个人证书到期，重新签名（或改用 TrollStore / 开发者证书） |
| 画面太大/太小 | 工具栏「缩放」切换三种模式；设置里可改插值 |
| 找不到 JAR | 确认扩展名是 `.jar`（不区分大小写），且放在 `Documents/J2ME/` |

## 4. 为什么 App 默认没有签名

CI 用 `CODE_SIGNING_ALLOWED=NO` 产出**未签名 `.app`**，再打成标准 IPA。
这样构建不依赖任何证书，任何人 fork 后立刻能出包；签名是可选项，
由 `scripts/sign-ipa.sh` 单独完成。TrollStore 恰恰需要未签名 IPA，
所以这个默认选择对 iOS 15 用户是最方便的。
