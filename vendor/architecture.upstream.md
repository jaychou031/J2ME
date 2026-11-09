# BBK 9588 phoneME/KVM J2ME 运行方案

> 更新：2026-08-09
> 当前主线：phoneME Feature MR2 / KVM
> 目标：在 BBK 9588 的 10 MiB 内存预算内运行《帝国时代3》等 MIDP 2.0 游戏。

## 1. 当前结论

正式运行时采用 phoneME Feature MR2；早期 NanoCoat/SquirrelJME 试验路线不再作为发布主线：

- CLDC/KVM、MIDP、LCDUI、PNG、RMS 等核心类 romize 后直接嵌入 BDA。
- 游戏由 9588 原生文件选择器选择，优先目录为 `B:\J2ME\`，没有 B: 时回退到 `A:\J2ME\`。
- 启动器读取 `META-INF/MANIFEST.MF` 的 `MIDlet-1`，不再硬编码游戏类名。
- 固件原始输入流负责触摸，原始六键包负责实体键；运行期不使用窗口触摸消息。
- phoneME RGB565 surface 直接提交到固件 framebuffer。
- 单 isolate Java 堆由上游默认 1280 KiB 调整为 3072 KiB。
- BDA 动态分配器硬上限为 6 MiB，避免 VM 失控挤占系统内存。
- 对老式 ZIP data descriptor JAR 在内存缓存中做容器规范化，不修改游戏类或资源内容。

2026-08-08 的 E 盘模拟器实测已经完成：文件选择、Manifest MIDlet 发现、语言选择、标题、
主菜单、关卡载入、实际地图和单位画面均可运行；实体确认键和方向键能送入 Java 游戏。

## 2. 架构

```text
J2ME.bda
  ├─ 9588 BDA shell
  │   ├─ 原生文件选择器（B:\J2ME\，无 B: 时 A:\J2ME\）
  │   ├─ frame 生命周期（事件泵仅用于最终 detach）
  │   └─ 有界内存分配与诊断
  ├─ phoneME Feature MR2
  │   ├─ KVM / CLDC 1.1
  │   ├─ MIDP 2.0 / LCDUI / GameCanvas
  │   ├─ PNG、JAR、Manifest、RMS
  │   └─ 3 MiB Java heap
  └─ BBK 9588 原生端口
      ├─ GUI+0x750 raw event → MIDP pointer event
      ├─ GUI+0x5d4 六键包 → MIDP game action
      ├─ 可拖动悬浮点 + 半透明手机键盘
      ├─ RGB565 surface → firmware framebuffer
      └─ PCSL file → A:/B: 固件文件系统
```

主线程由 phoneME master mode 驱动。运行期每轮最多消费 4 条固件原始事件，避免固件持续
产生的未知 `code=3` 饿死 VM。触摸 down/move/up（8/12/11）配合 `GUI+0x6c0` 最新坐标
进入定长原生事件队列；按住期间也直接轮询坐标缓存以补齐 move 间隙。关键按下/抬起事件
不会被移动事件挤掉。窗口事件泵不会在运行期参与输入，也不会在 wndproc 中执行 Java。

## 3. 内存方案

《帝国时代3》进入主菜单、载入关卡并显示实际地图时测得：

| 项目 | 字节 | MiB |
| --- | ---: | ---: |
| 正式 BDA 静态映像（含 CJK、软键盘与 MMAPI） | 2,104,524 | 2.01 |
| 动态分配峰值（含 3 MiB Java heap、JAR cache、MIDP surface） | 3,940,393 | 3.76 |
| 固件物理 framebuffer | 153,600 | 0.15 |
| 已核算合计 | 6,198,517 | 5.91 |

最保守上界按“静态映像 + 6 MiB 分配器上限 + 两块额外 240×320 RGB565 buffer”计算为
8,703,180 byte（8.30 MiB），仍低于 10 MiB。正常运行不会真的同时用满这个保守上界。

内存原则：

- Java heap 固定 3 MiB，不沿用桌面 JVM 的大堆默认值。
- 游戏 JAR 只缓存一次，上限 2 MiB；《帝国时代3》约 422 KiB。
- framebuffer 使用 RGB565，单帧 153,600 byte，不使用 XRGB8888。
- 文件读取使用 4 KiB 分块，启动后不重复从慢速 NAND 随机读取 JAR。
- 所有 `malloc` 都经过 BDA 计数分配器和 6 MiB 硬限制。

## 4. 文件布局

```text
B:\J2ME\                         游戏选择器优先目录
A:\J2ME\                         没有 B: 时的游戏目录
A:\应用\数据\J2ME\             用户可写数据默认目录
B:\应用\数据\J2ME\             存在用户数据时优先于 A: 同路径
  ├─ appdb\
  ├─ lib\skin.bin
  ├─ rms\
  └─ J2MEPHONE.LOG               启动/退出诊断日志
```

phoneME/KVM、MIDP 类库和 CJK 字库均已嵌入 BDA，所以不需要另放任何 runtime suite 或
系统 JAR。`应用\数据\J2ME` 只保存 RMS、appdb、自动生成的 `skin.bin` 和诊断日志：B:
同路径已有用户数据时继续使用 B:，否则自动创建并使用 A: 同路径。游戏 JAR 由文件选择器
选择，默认打开 `B:\J2ME\`；固件没有 B: 时打开 `A:\J2ME\`。

## 5. 游戏 JAR 启动

1. 原生选择器取得固件 GBK 路径。
2. VM 内部使用 ASCII 别名，避免中文文件名被错误当成 UTF-8。
3. PCSL 打开真实路径并把不超过 2 MiB 的 JAR 顺序缓存到 RAM。
4. 若 JAR 使用老式 16-byte ZIP data descriptor，则只在缓存中补齐 local header、移除
   descriptor 并修正 central directory offset。
5. phoneME 从 Manifest 解析 `MIDlet-1` 并启动对应类。

这解决了原始《帝国时代3.jar》的慢速随机读取和旧 ZIP 布局兼容问题。模拟器测试夹具只
重写了 ZIP 容器；游戏 class/resource 字节未修改。正式 BDA 会对用户选择的原 JAR 在
RAM 中完成同样处理。

## 6. 显示与输入

- 逻辑分辨率和物理输出均为 240×320 RGB565。
- `Canvas.paint()`/`GameCanvas.flushGraphics()` 最终提交到动态取得的固件 framebuffer。
- 不写死 framebuffer 地址；frame attach/detach 由 BDA 生命周期管理。
- 触摸使用 `bda_gui_raw_event_fetch()` 的 8/12/11 原始事件；坐标由
  `bda_gui_touch_position()` 读取，单轮最多消费 4 条。
- 实体上、下、左、右、确认、返回分别映射到 MIDP game action。
- 触摸产生的固件 Escape 毛刺在触摸有效期内过滤。

右上角默认显示半透明悬浮点。短按切换手机键盘；拖动时限制在屏幕边界内，键盘自动移到
悬浮点相反的半屏。绘制直径为 31px，触摸热区为 44×44；移动超过 12px 才进入拖动，
并在真机 UP 坐标失效时沿用最后有效坐标完成短按。键盘提供 `L=-6`、`R=-7` 和 ASCII `1–9/*0#` 的 MIDP 按键
down/up；按下态会高亮。顶部工具栏提供 `X`、`-`/`+`（主音量每次 10%）和 `M`（静音），
细条显示当前 0–100% 主音量。按 `X` 后调用 SDK `bda_confirm()` 显示固件原生“是/否”
模态框，只有返回 `BDA_DIALOG_RESULT_YES` 才发送 `SHUTDOWN_EVENT` 销毁 MIDlet 并安全
退出运行时；选择“否”后重绘浮层，面板外触摸继续传给游戏。

覆盖层展开时直接从 Java RGB565 screen buffer 合成一次到物理 framebuffer，并冻结该面板
区域；游戏仍持续更新背后的 Java surface，关闭面板时一次恢复最新画面。游戏每次 present
跳过已绘制的面板区域，因此不再对约 4 万个像素重复混色和命中测试。按键和工具栏
使用最多 6 个独立小脏矩形刷新，避免把相距较远的控件合并成大区域；大面积面板绘制每 8 行
主动补一次 PCM。拖动时只重写旧/新圆点的两个 31×31 脏区；仅跨越上下半屏时才搬移面板，
且不额外申请全屏缓冲。面板底板使用 224/256 黑色 alpha，普通按键使用 208/256 黑色 alpha，
文字和边框保持高亮白色。

phoneME 的西文字体保持原来的 9×14 advance；中文、中文标点、CJK 扩展 A、兼容汉字和
全角字符使用 14×14 点阵及 14px advance。仓库内提交了由 GNU Unifont 16px BDF 生成的
紧凑只读表；生成时裁去 1px 外边距，包含 28,400 个 1-bit 字形（766,812 byte），由二分
查找直接绘制到 MIDP screen/offscreen surface。正式构建直接嵌入并校验该文件，不再临时
下载字体源。字体来源声明为 SIL OFL 1.1 或 GPLv2+ with Font Exception。

《帝国时代3》的“开启声音 / 战役 / 单人对战 / 读取存档”、中文剧情、教程和地图单位名
均已在 E 盘模拟器验证可读；西文仍走 phoneME 原点阵，不受 CJK 字库影响。

退出时由 phoneME 先释放 Java screen buffer，返回原生入口后先停止全局毫秒定时器，再按
`frame_stop -> frame_release -> closing event pump -> close_frame` 完成固件 frame 的异步
detach。不能在 `midpFinalize()` 内直接关闭 frame，否则 BDA 虽然已经正常返回，系统 GUI
仍可能被旧 frame 占用而表现为黑屏死机。

## 7. 音频

phoneME ROM 内加入最小 MMAPI Player，仅实现老 J2ME 游戏常用的播放路径：

- `Manager.createPlayer(InputStream, "audio/midi")`：支持 SMF format 0/1、最多 16 轨、
  24 个同时发声音符、tempo/program/channel-volume/note/percussion 事件。
- `audio/x-wav` / `audio/wav`：支持 8/16-bit、单/双声道 PCM，运行时转为 22050 Hz、
  16-bit、mono。
- `Manager.playTone()`、`Player.start/stop/deallocate/setLoopCount/setMediaTime` 和
  `VolumeControl`。

原生层以 1024-byte 小块非阻塞写入 `bda_audio`，不依赖窗口消息或 GUI Timer。MIDI 使用
小型波形合成器，不打包 GM soundfont，因此新增代码/静态数据约 12 KiB，媒体数据按需申请
且单个音频流上限 512 KiB。打开 PCM 时把固件 attenuation 设为满音量，关闭时恢复打开前
的系统值；MMAPI `VolumeControl` 与悬浮菜单 0–100% 主音量作为两级数字比例，菜单默认
100%。暂不支持 MP3、AAC、AMR、流式网络音频或录音。

用户数据目录内的 `J2MEPHONE.LOG` 会记录 `AUDIO_MIDI_STARTS`、`AUDIO_WAV_STARTS`、`AUDIO_TONES/STOPS`、
`AUDIO_NOTES`、`AUDIO_WRITE_CALLS/BYTES/ERRORS` 与 `AUDIO_READY_MISSES`。

## 8. 构建与测试

正式构建：

```powershell
.\tools\bootstrap.ps1
.\tools\build-phoneme-bda.ps1 -AllocationLimitMb 6
```

模拟器自动启动《帝国时代3》的测试构建：

```powershell
.\tools\build-phoneme-bda.ps1 -AllocationLimitMb 6 -EmulatorAutoRun
```

关键可重放补丁：

- `tools/patch-phoneme-runmidlet-object.py`：唯一匹配并把 KVM 堆从 1280 KiB 改为 3072 KiB。
- `tools/patch-phoneme-rom-object.py`：修正嵌入式单 MIDlet 模式的 `Display.isShown()` 前台判定。
- `tools/build-cjk-font.py`：需要更新字库时，从 GNU Unifont BDF 重新生成紧凑 14×14 CJK 只读点阵表。
- `src/platform/bbk9588/phoneme_pcsl_file.c`：GBK 路径、JAR cache 和旧 ZIP 规范化。
- `src/platform/bbk9588/phoneme_cjk_text.c`：可变字符宽度、CJK 查找及 surface 绘制。

## 9. 发布闸门

已通过：

- E 盘 `bbk9588-emulator-v0.1.5` 冷启动。
- 原生 frame、触摸、实体键和 RGB565 输出。
- Manifest 自动发现混淆类名 `b`。
- 《帝国时代3》从启动到实际地图场景。
- 中文菜单、剧情、教程及地图单位名。
- 动态峰值 3.76 MiB、含完整 CJK/MMAPI 的保守总上界 8.30 MiB。
- OOM、未捕获异常和 VM 非零退出均未出现。
- 原始事件版在 v0.1.5 中可短按展开/收起；半透明面板持续 6 秒不被游戏刷新覆盖。
- 虚拟 `L` 软键已让真实《帝国时代3》从声音提示进入标题/主菜单。
- 深色单遍合成版在游戏启动动画中连续采样 50 帧，悬浮点 27 个特征像素每帧完整，
  `bad_frames=0`。
- 独立 MMAPI 探针完成 tone、MIDI、WAV 各一次：`AUDIO_STARTS=3`、`AUDIO_STOPS=3`、
  `AUDIO_WRITE_BYTES=90112`、`AUDIO_WRITE_ERRORS=0`；5 ms 非阻塞轮询将 ready miss 从
  724,817 次降到 267 次。
- 在 MIDI 播放期间展开优化后的完整软键盘，探针仍完成 tone、MIDI、WAV 各一次：
  `AUDIO_WRITE_BYTES=90112`、`AUDIO_WRITE_ERRORS=0`、`AUDIO_READY_MISSES=269`，与未展开
  浮层的 267 次基线基本一致。
- 真实《帝国时代3》开启声音后：`AUDIO_MIDI_STARTS=1`、`AUDIO_NOTES=1738`、
  `AUDIO_WRITE_BYTES=3826688`、`AUDIO_WRITE_ERRORS=0`；模拟器 DMA 无欠载、传输无丢包，
  游戏正常退出并执行 `AUDIO_STOPS=1`。
- 软键盘 `X` 使用 SDK 系统“是/否”框：选择“否”恢复游戏和面板；再次选择“是”后
  `STAGE=FINISHED`、`SOFTPAD_EXITS=1` 并返回系统菜单。

建议每个 Release 在真机继续回归：

- 30 分钟连续游戏和反复进出应用。
- 真机 NAND 读取速度、悬浮点连续 drag、声音设备。

用户数据目录内的 `J2MEPHONE.LOG` 在结束时会记录 `SOFTPAD_TOGGLES`、`SOFTPAD_KEY_EVENTS`、
`SOFTPAD_DRAGS`、`RAW_EVENT_FETCHES`、`RAW_TOUCH_EVENTS` 和 `RAW_TOUCH_MOVES`，可直接用于
真机验收。v0.1.5 模拟器的触摸 SADC 缓存只保留首次按下坐标，前端 move 不更新该缓存；
这属于模拟器输入模型限制，不应以窗口消息回退规避。
