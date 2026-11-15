# LOPPE 内置控件参考

> 📖 中文 | [English](LOPPE_BUILTIN_CONTROLS_EN.md)

---

## 1. 本文讲什么

界面节点树的每个节点都是一个控件，控件类型由标签名决定。本文逐个列出引擎内置的控件标签，给出每个控件的属性、默认外观、可用的伪状态、派发的事件、对应的 C++ 类型与运行期接口，以及最小示例。

**适用读者**：写 `.lui` 界面的人，以及需要在 C++ 里操作控件的人。前置条件是已按《从 C++ 到界面》把工程跑起来，或已读过《DSL 用户指南》第 2～4 节。

**内容导览**

| 你想找什么 | 去哪一节 |
|---|---|
| 所有控件共有的规则（伪状态、禁用、尺寸测量） | §2 |
| 一张控件总表 | §3 |
| 某个控件的属性与示例 | §4（按控件分小节） |
| 哪个控件派发哪个事件 | §5 |
| 使用中的限制与常见问题 | §6 |

---

## 2. 通用规则

### 标签与节点属性

- 标签名就是控件类型名（小写），一个节点只有一个类型：`panel`、`text`、`button`、`input`、`combobox`、`radio`、`progress`、`icon`、`image`。`scrollbar` 由引擎按需创建，一般不手写。
- 所有控件都接受通用节点属性：`id`（唯一标识，代码里按它查控件）、`class`（空格分隔的类名列表）、`value`（见各控件小节）。
- 所有控件都接受《DSL 用户指南》里的样式属性与布局属性：尺寸、内外边距、背景、边框、圆角、透明度、`display` / `flex-*` / `grid-*` 等。控件的"样子"由这些属性决定，不存在只能由 C++ 改的外观。

### 伪状态

伪状态只有四个，写在选择器末尾：

| 伪状态 | 触发条件 |
|---|---|
| `:hover` | 指针悬停在该节点上 |
| `:active` | 在该节点上按下 |
| `:focus` | 该节点持有输入焦点 |
| `:disabled` | 节点被禁用 |

同一节点同时满足多个条件时，按 `disabled > focus > active > hover` 取一个生效。

四个伪状态对所有控件都可用，但只有能获得焦点的控件（`input`）会真正进入 `:focus`；容器上的 `:hover` 会随命中测试一起生效。

禁用由 C++ 侧设置，没有 DSL 属性：

```cpp
control->setEnabled(false);   // 进入禁用态：不再悬停、不再按下、不再获得焦点
control->isEnabled();
```

### 默认外观

引擎为部分控件准备了默认样式（按钮、输入框、下拉框、文本），可以直接用，也可以在 `@style` 里覆盖。覆盖时按普通样式规则参与优先级计算：写在后面的规则、更具体的选择器胜出。

### 尺寸与内容测量

| 情况 | 行为 |
|---|---|
| `width` / `height` 写 `auto` 或不写 | 按内容测量：`text` 按文字、`image` 按图片原始像素 |
| 给了具体尺寸 | 按给定值；内容超出时（`text`）被裁在自身盒子里，不溢出到相邻控件 |
| 内边距与外边距 | 始终用 `border-box`：`padding` 与 `border` 包含在你声明的尺寸内 |

### 滚动条

- 需要滚动的控件会自己创建滚动条：多行输入框（`type="textarea"`）、内容超出且 `overflow` 为 `auto`/`scroll` 的容器。
- 滚动条粗细按宿主节点的样式属性 `scrollbar-width` 取值，未写时用一个细滚动条（默认 6）。
- 一般不需要手写 `scrollbar` 节点；确实需要时用 `ControlFactory::getInstance()->CreateScrollbar(方向)` 创建（`ScrollbarControl::Orientation::Vertical` / `Horizontal`）。

---

## 3. 控件总表

| 标签 | 作用 | 自有属性 | C++ 类型 | 派发事件 |
|---|---|---|---|---|
| `panel` | 容器：分组、卡片、布局载体 | — | `loppe::Panel` | 通用鼠标事件 |
| `text` | 显示文本 | `value` | `loppe::Text` | — |
| `button` | 可点击按钮（文字用子 `text`） | — | `loppe::Button` | `Click`、`MouseDown`、`MouseUp` |
| `input` | 文本输入（单行 / 多行） | `value`、`placeholder`、`type`、`wrap` | `loppe::Input` | `Input`、`Focus`、`Blur`、`KeyDown`、`KeyUp` |
| `combobox` | 下拉选择 | `options` / `arrow` | `loppe::ComboBox` | `Change` |
| `radio` | 单选圆点 | `checked`、`group`、`value` | `loppe::RadioButton` | 通用鼠标事件 |
| `progress` | 进度条 | `value` | `loppe::ProgressBar` | — |
| `icon` | SVG 图标 | `name` | `loppe::Icon` | 通用鼠标事件 |
| `image` | 图片 | `src` | `loppe::Image` | 通用鼠标事件 |
| `scrollbar` | 滚动条（引擎按需创建） | — | `loppe::ScrollbarControl` | `Scroll` |
| —（无 DSL 标签） | 软键盘（引擎自管理，触摸屏输入） | — | `loppe::SoftKeyboard` | —（直接写焦点输入框） |

"通用鼠标事件"指 `Click`、`MouseDown`、`MouseUp`，任何被命中的控件都会派发；事件语义见《从 C++ 到界面》§5。

---

## 4. 逐个控件

### 4.1 `panel` — 容器

**作用**：把一组控件装在一起，承担布局（`display: flex` / `grid`）、背景、边框、圆角、内边距。

**属性**：无自有属性。

**默认外观**：无背景、无边框；只有你写了才有。

**伪状态**：`:hover`、`:active` 会随命中生效（子控件被命中时父容器也在命中链上，事件会冒泡）；容器不获得焦点。

**C++ 类型**：`loppe::Panel`（继承自容器控件）。运行期接口 `setPadding(left, top, right, bottom)`（与 `AbstractControl::setPadding` 同序）与 `getPaddingTop/Right/Bottom/Left`；内边距通常直接写在样式里，不必调接口。

```loppe
@style {
    .card {
        background-color: #ffffff;
        border: 1px solid #e5e7eb;
        border-radius: 12px;
        padding: 16px;
        margin-bottom: 12px;
    }
}
@layout {
    .card { display: flex; flex-direction: column; }
}

panel(class="card") {
    text(value="卡片标题", class="card-title"),
    text(value="卡片正文")
}
```

### 4.2 `text` — 文本

**作用**：显示静态文本，不接收输入。

**属性**：`value`（文本内容）。

**样式相关**：`color`、`font-size`、`font-family`、`font-weight`、`line-height`、`text-align`，这些属性会从父节点继承。

**默认外观**：字号 14、颜色黑色、宽高按内容测量。

**行为**：宽度为 `auto` 时按文字内容测量；文字超出自身盒子时被裁掉，不会盖到相邻控件上。是否换行由布局上下文决定，`text` 没有换行属性。**只有一行的文字在自身盒子里垂直居中**（例如 `.menu-row { height: 28px }` 这种给了固定行高样式的情形）；折成多行的文字保持顶对齐，装不下时按上面说的裁剪。

内容里的 `\t`（制表符）按制表位排版：从显示行行首起每 `4 × 字号` 一个制表位，`\t` 占满到下一个制表位的那段空白（不画任何字形）。要对齐表格状文字就在 `value` 里写 `\t`，不要用一串空格。

**C++ 类型**：`loppe::Text`。

| 接口 | 说明 |
|---|---|
| `setText(std::string)` / `getText()` | 改 / 读文本 |
| `setFontSize(float)` / `setFontFamily(std::string)` | 字号 / 字体族 |
| `setBold(bool)` / `setItalic(bool)` | 字重 / 斜体 |
| `setLineHeight(float)` / `setAlignment(...)` | 行高 / 对齐（`Left` `Center` `Right`） |
| `setTextColor(ui::Color)` | 文字颜色 |
| `setWrap(bool)` | 是否自动换行（由布局决定，一般不必手动调） |

```loppe
text:total(class="total", value="共 12 项")
```

### 4.3 `button` — 按钮

**作用**：可点击的容器。按钮文字用子 `text` 节点给出，也可以放图标或其它子控件。

**属性**：无自有属性；尺寸、背景、边框、圆角、内边距都走样式。

**默认外观**（可直接用，也可覆盖）

| 项 | 值 |
|---|---|
| 背景 | `#007BFF` |
| 文字 / 边框 | `#FFFFFF` / 1px `#0056B3` |
| 圆角 / 字号 / 内边距 | 6 / 14 / `8 16` |

**默认伪状态**：`:hover` → 背景 `#0069D9`；`:active` → 背景 `#0056FF`、边框 `#003F7F`；`:disabled` → 背景 `#555555`、文字 `#AAAAAA`（**只换颜色**：默认规则不动字号 / padding / 圆角，禁用前后按钮几何不变）。

**事件**：`Click`（在同一按钮上按下并抬起）、`MouseDown`、`MouseUp`。

**C++ 类型**：`loppe::Button`（容器控件），没有专用接口；文字与外观走子节点和样式。

```loppe
button:submit(class="btn") {
    text(value="提交")
}
```

```cpp
onClick("#submit", [](const loppe::UIEventSystem::UIEvent&) { submit(); });
```

### 4.4 `input` — 文本输入

**作用**：单行输入框；`type="textarea"` 时为多行文本框。

**属性**

| 属性 | 值 | 说明 |
|---|---|---|
| `value` | 字符串 | 初始内容 |
| `placeholder` | 字符串 | 空内容时显示的占位文字 |
| `type` | `textarea` 或其它 | 写 `textarea` 走多行；其它值走单行 |
| `wrap` | `true` / `false` | 多行时是否自动换行（默认开） |

**默认外观**：背景 `#FFFFFF`、文字 `#000000`、边框 1px `#AAAAAA`、圆角 4、字号 14、内边距 `2 4`、**宽度 200**（要别的宽度就写 `width`）。

**默认伪状态**：`:focus` → 边框 `#80bdff` 并加一圈阴影。

**行为**

- 单击定位光标；双击选中词；三击选中整行；拖拽扩选。
- 支持选区、撤销 / 重做、长按退格与 Delete 重复删除。
- Tab 插入制表符（长按重复插入）；`\t` 按制表位排版，与 `text` 里的 `\t` 同一套规则。
- 滚动条**只在多行**出现：纵向在内容高于视口时出现，横向在关闭换行且内容宽于视口时出现；单行输入不显示滚动条，超宽文本靠光标跟随平移。
- 当前版本没有密码掩码：`type` 只区分单行与多行。

**事件**：`Input`（内容变化）、`Focus`、`Blur`、`KeyDown`、`KeyUp`。

**C++ 类型**：`loppe::Input`。

| 接口 | 说明 |
|---|---|
| `setValue(std::string, notify = true)` / `getValue()` | 改 / 读内容（`notify=false` 表示不派发 `Input` 事件） |
| `setPlaceholder(std::string)` | 占位文字 |
| `setType(std::string)` / `setWrap(bool)` | 单行 / 多行、是否换行 |
| `selectAll()` / `selection()` | 全选 / 读当前选区 |
| `setTextColor` / `setFontSize` / `setFontFamily` | 文字外观 |
| `textSearcher()` | 查找状态（查询串、命中集、当前命中项） |
| `onFocus()` / `onBlur()` | 焦点钩子 |

```loppe
input:name(placeholder="请输入姓名"),
input:note(type="textarea", wrap=true, height=120)
```

内容不会自动写回界面描述；需要保存时调用 `syncToAST(root, true)`（见《从 C++ 到界面》§6）。

### 4.5 `combobox` — 下拉选择

**作用**：展开弹层选择一项。

**属性**

| 属性 | 值 | 说明 |
|---|---|---|
| `options` | 字符串 | 选项列表，**英文逗号分隔**（选项本身不能含逗号） |
| `arrow` | 图标名 | 触发条右侧的展开箭头；缺省 `chevron-down`，写空串 `arrow=""` 则不画箭头 |

**默认外观**：边框 1px `#555555`；文字颜色与字号跟随样式（可继承）。触发条与展开项里的文字都在行内垂直居中；箭头是引擎内置图标（缺省 `chevron-down`，`arrow` 也可写 `chevron-up` / `chevron-right` / `chevron-left`），无需宿主注册。

**没有选项时**：弹层高度 = 选项数 × 30，`options` 为空时高度为 0 → 点了「像没反应」（占位文字需 `setPlaceholder` 设置，`placeholder` 属性不认）。选项要靠宿主 `setOptions` 灌（`options` 属性只给静态列表）。

**事件**：`Change`，`ev.text` 是选中项文本。

**C++ 类型**：`loppe::ComboBox`。

| 接口 | 说明 |
|---|---|
| `setOptions(std::vector<std::string>)` | **替换**整份列表（不是追加），可反复调用刷新；旧选中项若仍在新列表里则保持选中，否则选第一项 |
| `setSelectedIndex(int)` / `getSelectedIndex()` | 设 / 读选中位置（`-1` = 未选） |
| `getOptionCount()` | 选项条数 |
| `isOpen()` / `openPopup()` / `closePopup()` / `togglePopup()` | 弹层状态与开合 |
| `setPlaceholder(std::string)` | 无选中项时显示的文字（默认 `Select...`） |
| `setOnWillOpen(std::function<void()>)` | 弹层打开前的回调，适合在这里刷新选项（如设备列表热插拔） |
| `setArrowIcon(std::string)` / `getArrowIcon()` | 改 / 读展开箭头用的图标名（空 = 不画） |

```loppe
combobox:city(id="city", options="北京,上海,广州,深圳")
```

```cpp
onChange("#city", [](const loppe::UIEventSystem::UIEvent& e) {
    const std::string picked = e.text;
});
```

### 4.6 `radio` — 单选

**作用**：显示一个可选中的圆点。

**属性**

| 属性 | 值 | 说明 |
|---|---|---|
| `checked` | `true` / `false` | 初始是否选中 |
| `group` | 字符串 | 分组名 |
| `value` | 字符串 | 选项文字 |

**默认外观**：圆圈半径 8，边框 2；未选中为空心，选中时圆心填一个半径 4 的实心圆；悬停时描边变亮；指针在控件上变为手型。

**行为**：点击切换自身的选中状态。**当前版本的同组自动互斥尚未实现**：`group` 会被记录但不生效，需要互斥时在 `onClick` 里手动清掉同组其它项；`value` 只保存文字、不参与绘制，需要显示文字就在旁边放一个 `text` 节点。

**C++ 类型**：`loppe::RadioButton`。

| 接口 | 说明 |
|---|---|
| `setChecked(bool)` / `isChecked()` | 设 / 读选中状态 |
| `setGroup(std::string)` / `getGroup()` | 分组名 |
| `setText(std::string)` | 选项文字 |

不派发自有事件；点击时可用通用 `onClick` 捕获。

```loppe
panel(class="choice") {
    radio:r1(group="plan", value="基础版", checked=true),
    text(value="基础版")
}
```

```cpp
// 需要互斥时自己维护
onClick("#r1", [this](const loppe::UIEventSystem::UIEvent&) { selectPlan("r1"); });
```

### 4.7 `progress` — 进度条

**作用**：显示 0～1 的进度。轨道用控件自身的背景（`background-color`、`border-radius`），填充由控件自绘。

**属性**：`value`（声明取值，0～1；越界按 0 / 1 裁剪）。写了它界面一开始就是这个比例，不写则初值为 0、进度由宿主设置。填充色仍由 C++ 设置。

**默认外观**：填充色为蓝色 `rgb(59,130,245)` 左右（`0.23, 0.51, 0.96`），圆角收敛到半宽 / 半高以内。

**C++ 类型**：`loppe::ProgressBar`。

| 接口 | 说明 |
|---|---|
| `setProgress(float)` / `getProgress()` | 进度 0～1；超出范围自动钳位，等值调用不打断帧 |
| `setBarColor(ui::Color)` / `getBarColor()` | 填充色 |

不派发事件。填充是控件自绘的，**不要在宿主的渲染回调里再画一遍**。

```loppe
progress:bar(class="bar", value=0.42) { }   // 静态比例：直接写在描述文件里
progress:sync(class="bar") { }              // 运行期推进：不写 value，由宿主设置
```

```loppe
@style {
    .bar { width: 240px; height: 8px; background-color: #e5e7eb; border-radius: 4px; }
}
```

```cpp
if (auto control = getControlManager()->findControlById("sync"))
    if (auto bar = std::dynamic_pointer_cast<loppe::ProgressBar>(control))
        bar->setProgress(0.42f);
```

`value` 与 `setProgress` 的关系：声明了 `value` 之后又由宿主改过时，界面保持宿主的取值；
只有声明取值本身变了（改了 `.lui` 里的数字并重新解析）才重新应用 —— 窗口缩放、重新布局
都不会把运行期进度打回声明值。

### 4.8 `icon` — SVG 图标

**作用**：显示一个注册过的 SVG 图标，按当前 `color` 着色。

**属性**：`name` —— 图标的注册名。

**注册**：注册表由应用填充——启动时用 SVG 文本注册，界面里按注册名引用。同名重新注册会覆盖；内容里没有 `<svg` 的会被拒绝。引擎启动时（`Application::run`）已注册 `chevron-down` / `chevron-up` / `chevron-right` / `chevron-left`，其它名字要自己注册（没注册 = 什么都不画）。

```cpp
loppe::svg::RegisterIcon("close", "<svg viewBox=\"0 0 16 16\">…</svg>");
loppe::svg::HasIcon("close");   // 查是否已注册
```

**默认外观**：尺寸由 `width` / `height` 决定（不写则按图标自身尺寸）；颜色跟随 `color`，因此 `icon:hover { color: … }` 可以直接写。

**C++ 类型**：`loppe::Icon`：`setName(std::string)` / `getName()`。

```loppe
icon:closeIcon(id="closeIcon", name="close", class="icon")
```

### 4.9 `image` — 图片

**作用**：显示本地图片。

**属性**：`src` —— 图片路径。

**路径解析**（两个都常用）

```loppe
image(src="~/img/logo.png")   /* 相对当前 .lui 文件所在目录 */
image(src="pics/photo.jpg")   /* 相对资源根 */
```

- `~/` 需要宿主编好"当前界面文件目录"，做法见《从 C++ 到界面》§4。
- 其余相对路径按资源根、可执行文件目录依次查找。
- 只支持本地文件路径；网络地址不支持。`background-image` 用同一套规则。

**尺寸**：不写 `width` / `height` 时按图片原始像素参与测量；写了就按给定值绘制。

**加载**：异步加载，加载完成后引擎自行安排重绘，宿主不需要轮询。

**C++ 类型**：`loppe::Image`：`setSrc(std::string)` / `getSrc()`（运行期换图后会重新加载）。

```loppe
image:avatar(id="avatar", src="~/img/avatar.png", class="avatar")
```

### 4.10 `scrollbar` — 滚动条

**作用**：为需要滚动的控件提供滚动指示与拖动。

**谁创建**：引擎按需创建，一般在界面文件里不手写。触发场景：多行输入框、`overflow` 为 `auto` 或 `scroll` 且内容超出的容器。

**粗细**：宿主节点的样式属性 `scrollbar-width`，未写时用全局默认粗细（8）。

**单位是 dp，不是 px，更不是 sp**：布局、绘制、命中全链路都是 dp，渲染时乘平台 `ContentScale`。
所以「8」在 100% 缩放的桌面上是 8 物理像素，在 150% 缩放下是 12，在安卓 xhdpi 上是 16。
不要拿 Android 的 sp 来配滚动条 —— sp = dp × 系统字体缩放，滚动条跟字号走会让改字体设置时
控件宽度跳动；sp 只属于文字。

**按平台取不同默认值**：`ScrollbarControl::setDefaultThickness(dp)` 是运行时的进程级默认，
宿主在**建界面之前**按平台设一次即可（`<= 0` 或 NaN 恢复内置基准 8）：

```cpp
// 桌面：不写就是 8dp
// 安卓：触摸目标偏小，可以比桌面粗一点
loppe::ScrollbarControl::setDefaultThickness(12.0f);
```

优先级：实例 `setBarThickness()` > 节点样式 `scrollbar-width` > 全局默认（`defaultThickness()`）。
布局预留读的是同一份全局默认，所以「预留 = 绘制 = 命中/拖动」始终一致。

**恒占位**：容器写 `scrollbar-gutter: stable` 后，滚动轴无论内容是否溢出都固定预留一条条宽（预留就是它），X、Y 两轴各自独立，只对 `overflow` 为 `auto` / `scroll` 的轴生效；条本身仍只在真能滚时绘制。语义与取值见用户指南 §6.4。

**配色**：常态 `thumbColor()`（默认淡灰半透明），指针停在滑块上时取 `thumbHoverColor()`，默认 = `EmphasizeColor(thumbColor())` —— **RGB 不变、不透明度抬高**（α → α + (1−α)×0.72）。不透明度越高，与背景合成后的对比越大，所以深色底上变亮、浅色底上变暗，一套默认在两种主题下都看得见。只覆写 `thumbColor()` 时悬浮色自动跟随；要完全自定义就成对覆写两个。

**C++ 类型**：`loppe::ScrollbarControl`。

| 接口 | 说明 |
|---|---|
| `ScrollbarControl::Orientation::Vertical` / `Horizontal` | 方向 |
| `kScrollbarThickness` | 内置基准粗细常量（8dp，未调 `setDefaultThickness()` 时的取值） |
| `setDefaultThickness(dp)` / `defaultThickness()` | 全局默认粗细的运行时覆盖 / 读取 |
| `thumbColor()` / `thumbHoverColor()` | 常态 / 悬浮滑块色（protected virtual，子类覆写换配色） |
| `EmphasizeColor(color)` | 悬浮强调：同 RGB 抬高不透明度；原色已接近不透明时改为按亮度偏移 RGB |
| `ControlFactory::getInstance()->CreateScrollbar(方向)` | 手动创建 |

```loppe
@style {
    .list { overflow: auto; scrollbar-width: 8; }
}
```

---

### 4.11 软键盘（`loppe::SoftKeyboard`）

**作用**：触摸屏上的文本输入键盘。引擎自管理的一块屏幕底部面板，不进布局树、画在最上层；三平台（桌面 / 安卓 / 嵌入式）同一份实现，画的是通用 DisplayList 命令。

**行为**（默认全开，应用侧零代码）：

| 行为 | 说明 |
|---|---|
| 焦点自动弹出 | `input` 拿到焦点（输入源为触摸）→ 自动显示；焦点离开输入框 → 自动收起 |
| 触摸不穿透 | 落在键盘上的触摸被键盘吃掉，不改焦点、不点到底下的界面 |
| 出键直达 | 按键直接写进焦点输入框（与物理键盘同一条路，`Input` 事件照常派发） |
| ⇧ / ?123 / DEL | 大小写切换（打一个字自动回小写）、字母与数字符号两层、长按 DEL 连发 |

**C++ 接口**（`Scene::softKeyboard()` 取实例；键盘按需懒创建）：

| 接口 | 说明 |
|---|---|
| `show()` / `hide()` / `isVisible()` | 手动显示 / 收起（`show()` 期间不自动收起，`hide()` 解除） |
| `setEnabled(bool)` | 总开关（关闭后不显示也不自动弹出） |
| `setAutoShow(bool)` | 关掉焦点自动弹出，只听 `show()/hide()` |
| `setFontFamily(字符串)` | 键帽字体（空 = 平台默认） |
| `panelHeight()` | 键盘面板高度（随屏幕宽度缩放） |

**注意**：

- 桌面（鼠标输入源）不会自动弹出；需要时 `scene->softKeyboard()->show()` 手动唤出。
- 屏幕尺寸由 `ViewCoordinator` 自动同步（窗口 / 分辨率变化跟着走），宿主不用管。
- 嵌入式平台：`EmbeddedHost` 无需任何接线，焦点进 `input` 即弹。

---

## 5. 事件与控件对照

| 事件 | 含义 | 会派发的控件 |
|---|---|---|
| `Click` | 在同一控件上按下并抬起 | 所有可命中的控件 |
| `MouseDown` / `MouseUp` | 鼠标键按下 / 抬起 | 所有可命中的控件 |
| `Input` | 内容变化 | `input` |
| `Change` | 选项或选中项变化 | `combobox` |
| `Scroll` | 滚轮滚动 | 滚动容器、滚动条 |
| `Focus` / `Blur` | 获得 / 失去输入焦点 | `input` |
| `KeyDown` / `KeyUp` | 按键按下 / 抬起 | 获得焦点的控件 |

绑定的写法、冒泡与阻断规则见《从 C++ 到界面》§5。

---

## 6. 已知限制与常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| 按钮上的文字不显示 | 文字要写成子 `text` 节点，按钮自身没有文字属性 | 在 `button { }` 里放 `text(value="…")` |
| `input` 宽度总是一个固定值 | 引擎默认样式给了 `width: 200` | 在 `@style` 里显式写 `width` |
| 单行输入框没有滚动条 | 单行输入不显示滚动条 | 需要滚动条就用 `type="textarea"` |
| 两个 `radio` 同时选中 | 同组互斥尚未实现 | 在 `onClick` 回调里手动清其它项 |
| `radio` 旁边没有文字 | `value` 不参与绘制 | 在 `radio` 旁另放一个 `text` 节点 |
| `combobox` 选项被拆开 | `options` 按英文逗号分隔 | 选项文字里不要包含逗号；含逗号时用 C++ 的 `setOptions` 传列表 |
| `icon` 不显示 | `name` 没注册 | 启动时调用 `svg::RegisterIcon(名字, SVG 文本)` |
| 图片不显示 | 路径既不在 `.lui` 目录、也不在资源根或可执行文件目录下 | 用 `~/` 前缀或调整资源根；确认文件确实存在 |
| 禁用后仍在响应点击 | 禁用要设置在控件上 | 对控件调用 `setEnabled(false)`；禁用的节点不进入悬停 / 按下 / 焦点状态 |
| 进度条画了两层 | 宿主又手绘了一次填充 | 只用 `setProgress`，不要在渲染回调里重复画 |

---

## 相关文档

| 文档 | 内容 |
|---|---|
| `LOPPE_GETTING_STARTED.md` | 从空 C++ 工程到显示、交互、运行期改界面 |
| `LOPPE_CLI_GUIDE.md` | 用 `loppe-cli` 构建 / 校验 / 找产物，编译输出怎么读 |
| `LOPPE_MULTI_UI.md` | 一个工程多份 `.lui`、多个界面：挂载点、挂载 / 卸载、共享样式 |
| `LOPPE_DSL_USER_GUIDE.md` | DSL 语法、选择器、全部样式与布局属性 |
| `LAYOUT_DESIGN.md` | 尺寸模型、盒模型、flex / grid 的空间分配 |
| `LOPPE_CUSTOM_CONTROL_GUIDE.md` | 引擎没有的控件怎么自己实现 |
| `ai/LOPPE_DSL_AI_REFERENCE.md` | 属性全表 + 与标准 CSS 的关键差异（AI 用） |
