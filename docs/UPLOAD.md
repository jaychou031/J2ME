# 只用 iPhone 把工程传到 GitHub

> 适用条件：没有 Mac / PC，只有一部 iPhone 13（iOS 15）+ GitHub 账号。
> 全程在 **Safari 网页版** 完成，不需要装 Git、不需要敲命令、不需要电脑。

## 为什么这样做

工程有 80 多个文件，包含 `.github/workflows/`、`.gitignore` 这类隐藏文件，
还有二进制字库和需要保留可执行权限的 `.sh` 脚本。iPhone 的 Safari **不能上传
整个文件夹**，一个个新建 80 多个文件也不现实。

所以思路是：**你只上传 2 个文件**，剩下的交给 GitHub Actions 自己完成：

| 你手动做 | Actions 自动做 |
| --- | --- |
| ① 上传 `BBK9588-J2ME-iOS.zip`（1 个文件） | 解压 ZIP、把 82 个文件铺到仓库根目录 |
| ② 新建 `.github/workflows/bootstrap.yml`（粘贴一段文本） | 恢复 `.sh` 可执行权限、提交推送、删掉 ZIP |
| | 显式触发 `iOS 构建`，开始产出 IPA |

引导 workflow 的逻辑是自包含的，**不依赖 ZIP 里的任何脚本**，所以你手头
早期下载的那个 ZIP 一样能用（新 ZIP 只是多了本文档和模板）。

---

## 准备工作：把 ZIP 存到 iPhone

在对话里点开 `BBK9588-J2ME-iOS.zip` 卡片 → 分享 / 存储 →
存进 iPhone 自带的「**文件**」App（记住位置，比如「iCloud 云盘 / 下载项」）。

> GitHub 官方 iOS App **只能看 issue / PR / 通知，不能上传仓库文件**，别在它里面找上传入口。

---

## 第 1 步：Safari 切到「桌面网站」

移动端网页版 GitHub 没有「上传文件 / 新建文件」入口，必须切桌面版：

- iPhone Safari 打开 `https://github.com` 并登录；
- 点地址栏左侧的 **`AA`** 按钮 → 选「**请求桌面网站**」
  （或长按右上角刷新按钮 → 「请求桌面网站」）。

之后全程保持在桌面版网页，页面会很小，用双指放大操作。

---

## 第 2 步：新建空仓库

打开 `https://github.com/new`：

| 项目 | 填法 |
| --- | --- |
| Repository name | `BBK9588-J2ME-iOS` |
| Public / Private | 选 **Public**（公开仓库的 macOS runner 免费；私有仓库按 10 倍分钟扣额度） |
| Add a README file | **不勾** |
| Add .gitignore | **不勾** |
| Choose a license | **不勾** |

点 **Create repository**。空仓库即可，后面的引导 workflow 会把所有文件补齐。

---

## 第 3 步：打开 Actions 写权限（最容易漏，必做）

新建仓库的 `GITHUB_TOKEN` 默认是**只读**，引导 workflow 就没法把解压出来的
文件提交回去，会报 `Resource not accessible by integration`。

路径：仓库 → **Settings** → 左侧 **Actions** → **General** → 拉到最下面
**Workflow permissions** → 选 **Read and write permissions** → **Save**。

顺手确认上面的 **Actions permissions** 是 `Allow all actions and reusable workflows`。

---

## 第 4 步：上传 ZIP

仓库首页 → **Add file** → **Upload files**
（空仓库首页显示的是 `uploading an existing file` 链接，点它一样）

- 选择「文件」App 里的 `BBK9588-J2ME-iOS.zip`（899 KB，远低于网页上传上限）；
- Commit message 随便填，选 `Commit directly to the main branch` → **Commit changes**。

此时仓库里只有这一个 ZIP 文件。这一步的 push **不会触发任何构建**（仓库里还没有 workflow），正常。

---

## 第 5 步：新建引导 workflow（粘贴文本）

仓库首页 → **Add file** → **Create new file**

- **文件名**填：`.github/workflows/bootstrap.yml`
  （开头的点别漏；GitHub 会自动创建这两级目录）
- **内容**：把对话里的 `bootstrap.yml.txt` 打开 → 全选 → 复制 → 粘贴进来。
  **只有 20 行 / 约 1 KB**（模板：`docs/bootstrap-mini.yml`）。
- 选 `Commit directly to the main branch` → **Commit changes**。

> 手机上复制文本最稳的办法：先把 `bootstrap.yml.txt` 存到「文件」App，
> 用「文件」App 打开 → 长按 → 全选 → 拷贝 → 回 Safari 长按粘贴。
>
> 还有个更详细的 52 行版本 `docs/bootstrap-workflow.yml`，功能完全相同
> （多了日志与错误提示），粘哪个都能跑。

**这一次提交会立刻触发 `00 · Bootstrap`**，它会自动完成：

1. 解压 ZIP，把 `BBK9588-J2ME-iOS/` 里的全部内容（含 `.github/`、`.gitignore`、
   二进制字库）用 `cp -a` 铺到仓库根目录；
2. `chmod +x scripts/*.sh` 恢复脚本可执行位；
3. `git add -A` → commit → push；
4. 删除根目录的 ZIP（仓库里不再留 899 KB 冗余文件）；
5. 显式 `gh workflow run ios.yml` 启动真正的 iOS 编译。
   （GitHub 规定 `GITHUB_TOKEN` 产生的 push 不会再次触发 workflow，所以必须显式派发。）

---

## 第 6 步：等两个绿勾，下载 IPA

进仓库 **Actions** 页：

1. `00 · Bootstrap（展开手机上传的工程）` —— 约 1 分钟变绿。
   刷新仓库首页，应该能看到 `ios/`、`scripts/`、`docs/`、`.github/` 等完整目录，
   文件数约 85 个（含 2 个引导模板）。
2. `iOS 构建（iPhone 13 / iOS 15+）` —— 自动排队，macOS runner 可能等几分钟。
   默认 `kernel_backend=mock`，产出**可安装的诊断内核 IPA**。
3. 点进这次 run → 底部 **Artifacts** → `J2ME-iOS-IPA` → 下载
   `J2ME-unsigned.ipa`。

然后按 [`INSTALL.md`](INSTALL.md) 把 IPA 装到 iPhone 13（iOS 15 下最省事的是
TrollStore；其它版本用手机端签名工具或证书签名）。

> 之后你每次改代码 push，`00 · Bootstrap` 都会秒跳过（根目录已无 ZIP），
> 主构建照常自动跑。看它不顺眼可以在网页里删掉 `bootstrap.yml`，不影响构建。

---

## 排错

| 现象 | 原因 / 处理 |
| --- | --- |
| Bootstrap 报 `Resource not accessible by integration` | 第 3 步的写权限没开。去 Settings 改好后，Actions → `00 · Bootstrap` → **Run workflow** 重跑一次（远端 ZIP 还在，能重来） |
| Bootstrap 报 `ZIP 里没有顶层目录` | 上传的不是那个 ZIP，或 ZIP 被二次压缩过。重新上传原版 `BBK9588-J2ME-iOS.zip` |
| Bootstrap 变绿了但 `iOS 构建` 没出现 | 派发时机撞上索引延迟。手动点：Actions → 左侧 `iOS 构建（iPhone 13 / iOS 15+）` → **Run workflow** → 保持 `mock` → 运行 |
| 找不到 Add file / Upload files | 没切桌面网站，回第 1 步 |
| `iOS 构建` 红了 | 看 [`BUILD.md`](BUILD.md) 的排错章节，以及 artifact `phoneme-kernel-build-failure` |
| 手机下载 artifact 打不开 | `.ipa` 是压缩包，先存到「文件」App，再按 [`INSTALL.md`](INSTALL.md) 用 TrollStore / 签名工具打开 |

---

## 关于「把 ZIP 直接嵌进 yml」

这个想法方向对——**只提交一次**就完事。我确实做了，交付文件是
`bootstrap-embedded.yml`（放在工程 ZIP 之外，单独下载；ZIP 以 base64 heredoc
内嵌，workflow 运行时解码铺开，
带 `ios/project.yml` 幂等保护，展开后自动跳过）。但实测体积不适合手机：

| 做法 | 要粘贴进 GitHub 网页编辑器的文本 | 手机可行性 |
| --- | --- | --- |
| 完整 ZIP → base64 | **1.18 MB / 约 16,000 行** | ❌ 不可能 |
| 先 `xz -9e` 极限压缩再嵌 | 990 KB / 13,159 行 | ❌ 仍不行（字库熵高、PNG 已压缩，压不动） |
| 把 766 KB CJK 字库改成运行时 `curl` 下载 | 392 KB / 5,216 行 | ⚠️ 很卡，且依赖上游 raw 链接长期有效 |

原因有两个：iPhone Safari 里把上百万字符粘进 GitHub 的网页代码编辑器，
编辑器渲染巨大文本基本会卡死；GitHub 网页端对超大文件也不提供在线编辑能力。
**「一次提交」变成了「做不完的一次提交」**，反而不如两次轻量操作。

如果你后来有电脑 / 会用 GitHub API / 用 iOS 快捷指令调 REST 接口，
`bootstrap-embedded.yml` 就很好用：一次提交、不用上传 ZIP、不用开额外步骤。
本地已验证它能逐字节还原 85 个文件（CJK 字库 sha256 与原件一致）、
恢复脚本可执行位、提交推送并派发 `ios.yml`。展开后建议在网页里删掉这个
1.4 MB 的 workflow，免得每次 push 都要解析它。

**结论**：手机上用第 4 步上传 ZIP（点几下选文件）+ 第 5 步粘贴 20 行 yml，
是操作负担最小的组合。ZIP 那一步不必上传大文本，恰恰是手机最擅长的。

---

## 备选：手机端 Git 客户端（能折腾的话）

如果不想粘贴 YAML，也可以装 App 直接推完整目录，但步骤更多：

- **Working Copy**（App Store，iOS 上最成熟的 Git 客户端）：新建空仓库 →
  在 App 里 clone → 从「文件」App 导入 ZIP 并解压到仓库目录 → 全选 commit → push。
  注意它的 push 功能按当前政策**可能需要内购 Pro**，以 App 内提示为准。
- **iSH Shell / a-Shell**（免费，命令行）：`apk add git unzip` →
  在 GitHub 网页 Settings → Developer settings → Personal access tokens 生成
  一个勾了 `repo` 的 **PAT** → 在 App 里
  `git clone https://<用户名>:<PAT>@github.com/<用户名>/BBK9588-J2ME-iOS.git`
  → 把 ZIP 拷进来解压 → `cp -a BBK9588-J2ME-iOS/. .` →
  `git add -A && git commit -m init && git push`。完全免费，但手机上敲命令比较累。

这两条路推完就是完整仓库，不需要 bootstrap workflow，直接去 Actions 点
`Run workflow` 即可。

---

## 附：引导 workflow 做了什么（可核对）

完整源码在 [`bootstrap-workflow.yml`](bootstrap-workflow.yml)，核心是：

```bash
z=$(ls *.zip | head -1); [ -n "$z" ] || exit 0
unzip -q "$z" -d t && cp -a t/*/. . && rm -rf t "$z"   # 保留隐藏文件与权限位
chmod +x scripts/*.sh
git add -A && git commit -m init && git push origin "HEAD:$BRANCH"
for i in 1 2 3 4 5; do gh workflow run ios.yml --ref "$BRANCH" && break; sleep 3; done
```

它只认仓库根目录的 `*.zip`，自举完成后立刻 `exit 0`，不会动你后续的任何提交。
`gh workflow run` 是必须的：GitHub 规定 `GITHUB_TOKEN` 产生的 push 不会再触发
新 workflow，所以主构建要显式派发；重试 5 次是防止新 workflow 文件刚 push、
GitHub 尚未索引到。
