# esp32s3 — ESP32-S3 工程（ESP-IDF + loppe-ui 引擎预编译包）

由 IDE 的「New ESP32-S3 Project」生成。**构建真相是 ESP-IDF**，`build.lpj` 只让 IDE 认工程、
并把「本目标由 idf.py 产出」告诉构建器。

## 目录

| 路径 | 作用 |
|------|------|
| `build.lpj` | IDE 工程描述：机器侧事实（IDF 装在哪）+ 目标级 `build`（跑 `idf.py build`）|
| `CMakeLists.txt` / `sdkconfig.defaults` | ESP-IDF 工程（目标 esp32s3、PSRAM、C++ 异常/RTTI、主任务栈 16KB、**app 分区 1.5MB**）|
| **`main/main.lui`** | **界面**（唯一真相）：构建时编进固件；改它就能改界面，C++ 不用动 |
| `main/app_main.cpp` | 最小骨架：板级驱动 + 主循环（触摸采样 → `EmbeddedHost` → 按脏区刷屏）；引擎装配/脏区算法都在包里 |
| `main/lcd_st7789.{h,cpp}` | **板级**：ST7789 SPI 驱动（引脚/初始化按你的板卡改）|
| `main/touch_xpt2046.{h,cpp}` | **板级**：XPT2046 电阻触摸驱动（与 LCD 共用 SPI，只多占一根 T_CS）|
| `main/CMakeLists.txt` | 板级代码编成一个 IDF 组件（`*.cpp` 用 GLOB 收）+ `EMBED_FILES "main.lui"`，并链接下面的引擎包 |
| `main/font_chars.txt` + `tools/generate_font.py` | **中文界面要显示的字 + 字库生成脚本**（见下面「中文界面」；`main/embedded_font.cpp` 是它生成出来的）|
| `../thirdparty/loppe-embedded-v1.0.0-preview.2/esp32/` | **引擎预编译包**：`lib/libloppe.a` + `include/`（随工程走，可换新导出的包；布局同 IDE 新建的工程）|
| `AGENTS.md` | 给 AI 助手的说明（先读哪份文档、改哪里）|
| `docs/loppe-ui/` | 引擎文档（人读在根，AI 速查在 `ai/`；新建工程时拷进来）|

引擎（布局/渲染/控件 + `embedded/platform` + 位图字库）都在那个包里，本工程**没有引擎源码**，
也不引用引擎源码路径。引擎的 C++ API 在包里是「一个总入口 + 每目录一个汇总头」：
`#include "loppe.h"` 拿全部；只想要某个模块就用
`#include "engine/loppe_engine.h"` / `"components/loppe_components.h"` / `"render/loppe_render.h"` …
（本工程的 `platform/embedded_host.h` 内部就 include 了 `loppe.h`，所以 `app_main.cpp` 不用再管这些。）
需要更新引擎：在 loppe-ui 源码树里跑
`bash thirdparty/loppe-ui-src/export-esp32.sh` 重新导出，再把新包覆盖过来。

## 界面：`main/main.lui`（改它就行）

```
main/main.lui   ← 界面（DSL：@style / @layout / 控件树）
main/app_main.cpp ← 每帧三件事：s_touch.read() → s_host.touchSample()+tick() → 按脏区刷屏
```

- `.lui` 由 `main/CMakeLists.txt` 的 `EMBED_FILES "main.lui"` 编进固件，`app_main.cpp` 里用
  `extern const uint8_t kMainLuiStart[] asm("_binary_main_lui_start")` 取字节（符号名 = 文件名 + `_start`/`_end`）。

### 界面不止一份 `.lui`

`main/main.lui` 是模板给的那一份，**不是上限**。工程要放多个界面时：

```
main/main.lui      ← 外壳（顶栏、标题…）
main/page_home.lui ← 页面
main/page_set.lui  ← 页面
```

1. `main/CMakeLists.txt` 的 `EMBED_FILES` 多列几份（路径相对组件目录）：
   `EMBED_FILES "main.lui" "page_home.lui"` → 符号 `_binary_page_home_lui_start` / `_end`。
2. `app_main.cpp` 里每份加一对 `extern ... asm("_binary_<路径里的 / 换成 _>_start")`，
   用 `std::string(reinterpret_cast<const char*>(begin), end - begin)` 取文本。
3. 两种切法：
   - **整屏换**：`s_host.loadUi(文本)` 再调一次（解析失败返回 false，屏幕保持原样）。
   - **外壳 + 每页一份**（推荐）：`main.lui` 末尾留 `panel(id="page-slot") {}`，
     页面用 `ComponentInstantiator::SpawnElementViewUnderId(s_host.scene(), "page-slot", 页面文本,
     &s_host.runtime())` 挂上去；换页前对上次返回的 `ElementView` 调 `remove()`。

细节（挂载点高度、卸载、多文件共享样式、常见错误）：`docs/loppe-ui/LOPPE_MULTI_UI.md`。
- 想加交互：在 `app_main.cpp` 里绑（选择器 = `#id`，跨控件重建依然有效）：
  `s_host.scene()->onClick("#btn-hello", [](loppe::UIEventSystem::UIEvent&) { … });`
  改文字：`findControlById("lb-hello")` → `dynamic_cast<loppe::Text*>` → `setText(…)`。
- ⚠ 屏上的字来自**离线生成的位图字库**：字库里没有的字画成 `?` —— 字库由构建自动生成/更新（见下一节）。

## 中文界面：字库（构建时自动生成）

屏上的字来自 `main/embedded_font.cpp` 这份位图字库（**字库里没有的字画成 `?`**；字号也是按套生成的）。
它是 `tools/generate_font.py` 生成的，**构建时会自动跑一次**（`build.lpj` 里 `build` 的第一条命令）：

- 改了 `main.lui` 的文案 → 把新字写进 `main/font_chars.txt` → 直接重新构建，字库自己重生成；
- 输入没变（字符表 / 字号 / 字体都没动）→ 脚本跳过，不重生成、不重编。

字符表怎么填：`main/font_chars.txt` 一行或几行都行（重复的字不用写；`#` 开头的行是说明注释）。
文件里**预置了一行常用界面词**（确定/取消/返回/设置…）：用不到就删掉（省 flash），
要显示的字直接往上补。英文/数字/标点会自动带上，不用写。

字号**自动取 `main.lui` 里用到的 `font-size`**（引擎按字号精确匹配，不插值）—— 不用手写。

手动跑（工程根目录）：

```
python tools/generate_font.py --if-changed    # 输入没变就跳过（构建时用的就是这条）
python tools/generate_font.py                 # 立即重生成，不判增量
```

> 用的是**电脑上的** Python，要装 Pillow：`pip install pillow`；哪个 python 由 `build.lpj` 顶部的
> `HOST_PYTHON` 指定（默认 `python` = 走 PATH）—— **不是** IDF 那个（它没装 Pillow）。

常用选项：

| 选项 | 什么时候用 |
|------|-----------|
| `--chars "温度 湿度"` | 临时补几个字，不想动字符表 |
| `--sizes 12,14,16` | 额外加字号（默认已含 `main.lui` 里用到的那些）|
| `--font D:/fonts/xxx.ttf` | 换字体（想换字重就指到自己喜欢的 .ttf/.ttc）|
| `--output <文件>` | 生成到别处，不动 `main/embedded_font.cpp` |

提示：

- 工程名也画在屏上（`main.lui` 的 `value="esp32s3"`）——工程名是中文的话，把它也写进字符表。
- 字号越少越省 flash（每个字号一套点阵）：字号是自动取 `main.lui` 里用到的，别在 `.lui` 里写用不到的字号。
- 不想让字库自动生成：删掉 `build.lpj` 里那条 `build` 命令，改成手动跑脚本。
- DSL 语法（`@style` / `@layout` / 控件树三段）与可用属性：查工程里的 `docs/loppe-ui/`
  （`ai/` 下是 AI 速查，其余是用户指南 / 控件清单 / 布局 / 自定义控件）；拿不准就先照
  `main/main.lui` 改样式和文案试。

## IDE 里能看到什么（`build.lpj` 只负责「显示」，不参与编译）

本工程的**构建真相是 ESP-IDF**，所以 `build.lpj` 里**没有 `sources`**（写了反而会和目标级
`build` 模板冲突）。要在 IDE 的项目树里看到文件，靠目标里的一行 `view`：

```
view = [ main/**.c, main/**.cpp, main/**.h,          # 板级源码（glob；新增源文件自动出现）
         main/main.lui,                             # 界面（编进固件，但这里只是「显示」）
         CMakeLists.txt, main/CMakeLists.txt,       # 构建真相
         sdkconfig.defaults, README.md ]            # 配置 / 文档
```

- `view` = **仅显示文件**：写法与 `sources` 一样（相对本文件目录、`**` 递归、支持 `*`/`?`，
  也支持直接写文件名），树里列出（**与源码同色**）、点开照常编辑 —— **不进构建清单、不参与编译**。
- 编不编、怎么编，完全由 IDF（`CMakeLists.txt` + `main/CMakeLists.txt`）决定：`main/` 下
  （含子目录）的 `.c` / `.cpp` 自动编进固件，新增源文件不用改任何清单。
- 匹配不到文件的模式会自动跳过（删文件后不用回来改这一行）。加了别的源码目录（如 `components/`）
  照样子往这行追加。
- 想看工程里的**所有**文件（包括 `thirdparty/` 里的引擎包）：侧栏切到「全部文件」。

## 机器侧事实（换机器 / 升级 IDF 只改这几行）

`build.lpj` 顶部：

```
IDF_ROOT   = D:/Espressif
IDF_PYTHON = ${IDF_ROOT}/python_env/idf5.4_py3.11_env/Scripts/python.exe
IDF_PY     = ${IDF_ROOT}/frameworks/esp-idf-v5.4.4/tools/idf.py
IDF_NINJA  = ${IDF_ROOT}/tools/ninja/1.12.1
IDF_XTENSA = ${IDF_ROOT}/tools/xtensa-esp-elf/esp-14.2.0_20260121/xtensa-esp-elf/bin
```

取这些路径的权威办法：`<IDF python> <IDF_PATH>/tools/idf_tools.py export`。

## 触摸（XPT2046 电阻屏）

工程自带一块 XPT2046 电阻触摸驱动，界面里按钮的 `:hover`/`:active` 就是它的实测反馈
（触摸 → 鼠标事件由包里的 `EmbeddedHost::touchSample()` 翻译，`app_main.cpp` 每帧喂一次采样）。
屏上除了 LCD 那排线，还引出 5 根触摸线：`T_CLK / T_DIN / T_DO / T_CS / T_IRQ`。
**触摸和 LCD 共用同一条 SPI 总线**（ESP-IDF 里 mode/时钟是「每设备」的，XPT2046 用 mode 0 + 1MHz，
LCD 保持 mode 3 + 20MHz，互不干扰），所以只有 `T_CS` 要单独占一根引脚：

| 屏上丝印 | 接到 ESP32-S3 | 说明 |
|---|---|---|
| `T_CLK` | GPIO12 | 和 LCD 的 SCLK 并在一起 |
| `T_DIN` | GPIO11 | 和 LCD 的 MOSI 并在一起 |
| `T_DO` | GPIO13 | 和 LCD 的 MISO 并在一起 |
| `T_CS` | GPIO14 | **单独一根片选**，不能和 LCD 的 CS（GPIO10）重复 |
| `T_IRQ` | 可选（如 GPIO15） | 不接也能用（靠采样值判断按下）；接了就把 `touch_xpt2046.cpp` 里的 `PIN_TOUCH_IRQ` 改成对应 GPIO |
| `VCC` / `GND` | 3V3 / GND | 模块上一般和 LCD 共电源共地 |

引脚都在 [main/touch_xpt2046.cpp](main/touch_xpt2046.cpp) 顶部的宏里，按你的板卡改。

### 触摸是怎么接到界面引擎上的

引擎的事件入口就是鼠标事件，所以触摸按住 = 鼠标左键按下、抬起 = 松开，坐标直接用像素
（本工程 devicePixelRatio 恒为 1，逻辑 DIP == 像素）。这一层翻译**由引擎侧的
`EmbeddedHost::touchSample(touched, x, y)` 做**（包里的 `platform/embedded_host.h`），
宿主每帧把驱动给的采样结果喂进去一次：

```cpp
s_host.touchSample(touched, x, y);   // 有触摸：onMouseMove（hover 跟手）→ 首帧 GLFW_PRESS
                                     // 没触摸：在最后一点 GLFW_RELEASE（发 click）+ clearPointerState()
```

界面上的按钮直接绑回调即可（选择器支持 `#id` / `.class`）：

```cpp
s_host.scene()->onClick("#btn-count", [](loppe::UIEventSystem::UIEvent&) { /* ... */ });
```

> **别拿落笔第一帧的坐标去按下**：电阻屏刚落笔那几帧坐标会乱跳。若用这一帧派发按下，按下就落在
> 别处 —— 按钮不进 `:active`、抬起时也对不上，click 永远不触发（真机现象就是「只有 hover，
> 按下没反应、数量不加」）。这个坑**由驱动解决**（`touch_xpt2046.cpp`），app 侧照驱动给的
> touched 直接翻译，**按下/抬手都不去抖**：
>
> | 时机 | 动作 | 为什么不去抖 |
> |---|---|---|
> | 驱动报「有触摸」的第一帧 | `onMouseMove`（hover 跟手）→ 立刻 `GLFW_PRESS`（`:active` 当帧变绿） | 等「位置稳住」会让快点一下不亮（按下与抬起被挤到同一帧，`:active` 从没被画出来） |
> | 驱动报「没触摸」的那一帧 | 在最后的手指点 `GLFW_RELEASE`（发 click）→ `clearPointerState()` | 要求「连续 2 帧读不到才算抬手」会把**连点**吞掉：连点时抬手极短 → 测不到 → 按下状态一直挂着 → 后面的点击都不来 |
>
> 一帧噪声不该被当成一次松手 —— 这件事也归驱动：**按着的时候样本不可信就沿用上一帧坐标**
> （接触电阻 z 带滞回：`z ≥ TOUCH_Z_MIN(400)` 算按上、`z < TOUCH_Z_RELEASE(200)` 才算松手），
> 所以驱动报的 touched 本身就是可信的「真按着 / 真松手」。

> **每帧必须消费「更新计划」**：引擎把「这一帧要重排/重绘什么」记在 `UpdateScheduler` 里。
> 这一步由 `EmbeddedHost::tick()` 承担（它调**引擎自带的** `ViewCoordinator::apply()`；预编译包里就有
> `view_coordinator.cpp`，桌面端同一个类）—— 管道、控件同步、脏区记录都在里面，宿主**不要手抄**。
> **漏了这步，`:hover`/`:active` 的样式和运行时文本更新都不会出现在屏幕上** ——
> 表现也是「按了像没反应」。

### 怎么测（按下/悬浮的观感）

模板界面只有 `main/main.lui` 里那一屏（标题 + 两行说明 + 一个按钮 `#btn-hello`），
按钮的按下/悬浮观感全靠 DSL 的 `:active` / `:hover` 伪类。烧进去后：

| 现象 | 说明 |
|------|------|
| 手指按在按钮上 | 按钮**当帧**变绿（`.btn:active`）—— 触摸反馈；松手回到深蓝 |
| 手指停在按钮上没抬 | 变浅蓝（`.btn:hover`） |
| 手指抬起 | 调 `Scene::clearPointerState()` 把 hover/active 清掉（触摸屏没有「鼠标移出」，不清会留色） |

> **模板不含自检叠层/诊断日志**（2026-10-09 精简）：红准星、右上角帧率、左上角计数方块、
> 命中目标日志、每秒一行原始数据等，都在模板里删掉了 —— 它们属于「调板子时临时加」的东西。
> 要诊断就自己加两行（`ESP_LOGI` 打 `s_touch.read()` 的原始值；或在 `tick()` 之后往
> 帧缓冲画一块叠层，先 `s_host.markDirty(x, y, w, h)` 把这块标脏），改完删掉即可。

### 校准（按下的位置和手指对不齐时才需要）

[main/touch_xpt2046.cpp](main/touch_xpt2046.cpp) 顶部的标定宏，分两类：

| 宏 | 含义 |
|---|---|
| `TOUCH_CAL_X_MIN` / `TOUCH_CAL_X_MAX` | X 轴原始值的两个端点：按住屏幕左、右边缘各读一次，**小的填 MIN、大的填 MAX**（MIN/MAX 只表示原始值大小，不代表屏幕左右）|
| `TOUCH_CAL_Y_MIN` / `TOUCH_CAL_Y_MAX` | Y 轴原始值的两个端点，按上、下边缘同理 |
| `TOUCH_CAL_SWAP_XY` | **轴交叉**：手指左右动、屏幕上的按下位置上下动 → 改成 `true` |
| `TOUCH_CAL_INVERT_X` / `TOUCH_CAL_INVERT_Y` | **单轴镜像**：手指往左、按下位置往右（或上下反）→ 翻转对应那一个 |
| `TOUCH_CAL_OFFSET_X` / `TOUCH_CAL_OFFSET_Y` | **整体偏移（像素）**：按下位置比手指固定偏一点点（常见于便宜面板；手指接触点与 LCD 像素本来就不重合）→ 用这两个微调，别去动 MIN/MAX |

原始值自己打日志读（`ESP_LOGI` 打 `s_touch.readRaw(x, y)` 的原始值，或用 `s_touch.trace()`）。
顺序：先接上正确的 `SWAP_XY`，再逐个翻 `INVERT_*`，最后填 MIN/MAX 把边缘对正（拿界面上那个按钮当靶子：
手指按哪，看它在哪变绿）。
模板现在的默认值（`SWAP_XY false` / `INVERT_X true` / `INVERT_Y false`）
是按 240x320 通用 SPI 模块实测的方向设的，换屏或换个装法才需要动。

### 帧率与「为什么整屏刷只有 9 帧」

**9 帧不是 CPU 慢，是刷屏带宽把它按住了**：

| 项 | 数字 |
|---|---|
| 整屏像素 | 240 × 320 = **76800 px** |
| 帧缓冲字节 | 76800 × 2 = **153,600 B** |
| 20MHz SPI 传输 | 153600 × 8 / 20e6 ≈ **61 ms** |
| 加上渲染 + 节拍 | ≈ 65（刷屏）+ 25（软渲染）+ 16（节拍）≈ **106 ms → 9 帧** |

**LVGL 之所以看着高，不是渲染快，而是它只刷「变化的区域」**（`disp_drv.flush_cb` 拿到的是脏矩形，
常常只有几 KB）。同样的面板、同样的 SPI，LVGL 整屏刷也只能到 ~16fps。

本工程做同一件事：**脏区由引擎算 + 由引擎合并**（`EmbeddedHost` 里的 `loppe::ui::DirtyRegion`：
`ViewCoordinator` 把 `PartialUpdate` 的脏节点外框记下来，重建/重排/纯重绘报「整屏」；宿主再补上
外扩 margin / 裁剪到屏 / 相交合并 / 块数上限 / 面积阈值），宿主只做 `flushRegion` 那几块 ——
按住的按钮 ≈ 220×46 ≈ 20KB ≈ 8ms，比整屏快 8 倍。另有两条：**没有变化就整帧不刷**（不重画也不传，
省电）、**忙时把 16ms 节拍压到 1ms**。策略参数在 `app_main.cpp` 的 `Config` 里（跟刷屏带宽绑）。

配套（引擎侧，2026-10-08 一起改的）：

- 事件系统不再无脑标脏「悬停路径上的所有节点」——只标**真写了 `:hover`/`:active` 规则**的节点
  （否则祖先的整屏矩形会把脏区优化冲掉）；
- 祖先的状态规则若改了**可继承**属性（`color`/`font-*`），其子树会自动跟着重同步
  （`.btn:hover { color }` 才真的能改到按钮里的文字）。

保守兜底（宁可多刷，绝不漏刷留残影）：首次 / `ASTRepaint` / `Repaint` / `FullLayout` / `LayoutOnly`
/ `PaintOnly` → 引擎报整屏；块数超 `maxDirtyRects`（模板 4）/ 脏区总面积超 `fullFlushPercent`
（模板 60%）→ 宿主整屏刷。想量帧率就自己加个叠层（照上面「怎么测」那段的做法：往帧缓冲画字 +
`s_host.markDirty(...)`）。

**还要更快？** 剩下三笔（性价比序，都还没做）：

1. **抬 SPI 时钟**：[main/lcd_st7789.cpp](main/lcd_st7789.cpp) 的 `devcfg.clock_speed_hz = 20 * 1000 * 1000;`
   很多 ST7789 模块能吃 40MHz（刷屏 61→31ms）；长排线可能花屏，先试再定。
2. **只重画脏区**（不只重刷）：现在每帧仍把整屏软渲染进帧缓冲（PSRAM 写 ≈ 25ms）。
   引擎有 `Renderer::render(displayList, target)`（带裁剪），但要先把「绝对坐标的 DisplayList
   裁到某个矩形」这条路验证清楚（现在它是给「按目标矩形贴一张局部坐标图」用的）。
3. **刷屏与渲染重叠**（双缓冲 + SPI DMA 异步刷）：一帧时间从「渲染+刷屏」变成
   `max(渲染, 刷屏)`；LVGL 的双缓冲就是这么干的，代价是两份帧缓冲（2×153KB PSRAM）。

### 「没摸屏幕也在动」= 悬空读数（已按标准做法处理）

电阻屏**没被摸时 X/Y 极板是悬空的**，ADC 读出来的是乱值、还常贴着一端 → 映射到屏幕角落
（真机现象：按下位置对不上、click 永远不触发）。所以光看「X/Y 在量程内」判断不了有没有按。
驱动里因此做了两件事（都在 [main/touch_xpt2046.cpp](main/touch_xpt2046.cpp)）：

| 机制 | 说明 |
|---|---|
| **接触电阻判定**（`TOUCH_USE_PRESSURE` / `TOUCH_Z_MIN`）| 读 XPT2046 的 Z1/Z2：`z = z1 + 4095 - z2`，**z < TOUCH_Z_MIN 就当没摸**。没摸时 z1 小、z2 接近满量程 → z 小；摸上时 z 明显变大。阈值 400 是各家 XPT2046 驱动通用的经验值 |
| **帧内样本一致性**（`TOUCH_SPREAD_MAX`）| 同一帧 4 个样本差得离谱就是噪声，不当触摸 |

拿不准阈值时自己打日志看原始值（`s_touch.trace()` / `readRaw()`）：没摸时的 z 与按上去时的 z
差得越多越好调；`TOUCH_Z_MIN` 调大更严（更不容易误判为按上），调小更灵敏。

> 更省事的办法：把触摸排线上的 `T_IRQ` 接到一个 GPIO（如 GPIO15）并把 `PIN_TOUCH_IRQ` 改成它 ——
> XPT2046 按下时会拉低 T_IRQ，硬件级「有没有按」，比压力阈值更可靠（驱动里已支持，接了就生效）。

## 构建

IDE 里直接构建（等同下面这条命令，环境变量由 `build.lpj` 的 `env` 注入）：

```
"<IDF_PYTHON>" "<IDF_PY>" -C . -B build build
```

产物：`build/esp32s3.elf` 与 `build/esp32s3.bin`（可烧写镜像）。
构建目录就是 IDF 缺省的 `build/`（本工程没有桌面模板目标，构建器只在该目录下留一个空的
`<配置>/` 目录，不冲突）。

参数与产物位置：`docs/loppe-ui/LOPPE_CLI_GUIDE.md`。

## 烧写 / 串口监视

**不是 build 动作**（要设备/串口参数，不进 `build`/`post_build`）。

因为构建目录就是 IDF 缺省的 `build/`，在工程根目录直接敲就行，**不需要 `-B`**：

```
idf.py -p COMx flash monitor
```

（同一条命令的完整写法：`"<IDF_PYTHON>" "<IDF_PY>" -C . -p COMx flash monitor` ——
`idf.py` 缺省 `<工程>/build` 就是 IDE 编出来的那个目录，所以 `flash` 前的 build 步骤是
no-op，不会重编。若你手动传了别的 `-B`，那才会重新配置 + 全量重编。）

## 共享内核（../kernel/src）

`main/CMakeLists.txt` 已经把 `../kernel/src` 的源码和 `../thirdparty/quickjs-ng` 四个 .c
一起交给 IDF 编（不产 `.a`、不拷副本），和桌面壳、安卓壳是同一份内核。

- 目录关系：`esp32s3/` 和 `kernel/` 是兄弟目录；
- 界面仍然是模板（`main/main.lui`）—— 还没有把 `BrowserScene` 挂上；
- 下一步：在 `main/app_main.cpp` 里实现 `browser_host.h` 的 `BrowserHost`
  （drawer / textRenderer 用 `EmbeddedHost` 那套，`views().attach` 接到板级 Surface），
  再 `scene->setHost(this)`；同时确认 QuickJS 的内存上限与 PSRAM 余量。
