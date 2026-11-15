# LOPPE DSL 语法参考（AI 版）

> 紧凑版，用于注入 AI 上下文。属性全集、语法规则、与标准 CSS 的关键差异。

> 📖 中文 | [English](LOPPE_DSL_AI_REFERENCE_EN.md)

> 构建 / 运行：一律 `loppe-cli build . Debug`（改 `build.lpj` 后先 `loppe-cli config .` 校验）。
> 工程没有 `CMakeLists.txt`，不要用 cmake / make / msbuild，也不要手敲 clang 命令行。
> 参数与产物位置见 `../LOPPE_CLI_GUIDE.md`。
>
> **界面文件不限于一份**：一个工程可以有多份 `.lui`（外壳留 `panel(id="…")` 当挂载点，
> 每页一份文件，运行期用 `ComponentInstantiator::SpawnElementViewUnderId` 挂上去）。
> 拿到工程先看界面目录里有几份 `.lui`、C++ 里有没有这类挂载/多次 `loadUi`，别默认只有 `main.lui`。
> 做法、卸载、共享样式、ESP32 的 `EMBED_FILES` 多文件写法见 `../LOPPE_MULTI_UI.md`。
>
> **`.lui` 里没有模板 / 占位语法**：不要写 `@ROWS@`、`@ITEMS@` 这类标记再在 C++ 里字符串替换 ——
> `@` 只认 `@style` / `@layout` / `@version`，其它一律解析报错。列表行要么在 C++ 里拼好行 DSL
> 挂到空容器（`SpawnElementViewUnderId`）、要么用 `ComponentHost::SetRows`、要么自绘；
> 详见 `../LOPPE_MULTI_UI.md` §2.6。

---

## 1. 快速开始

LOPPE DSL 文件由三种块组成：

```loppe
@version(1.0)                      // 可选：本文件要求的界面 DSL 版本（只比主版本；不写 = 不检查）
@style    { /* 样式规则 */ }
@layout   { /* 布局规则 */ }
// 节点树（控件层级）
panel(id="root") {
    text(value="Hello")
}
```

`@style` 和 `@layout` 块必须出现在节点树之前。`@version(...)`（可选）声明界面 DSL 版本：**写法只有一种**
—— `@version(1.1.0)`（括号 + 裸版本号，纯数字段、不带引号；不写括号或写成 `@version("1.1.0")` 都报错）；
与引擎主版本一致才通过，不一致解析直接报错（当前引擎版本见 `loppe::engineVersion()`，其版本串可带
`-preview.1` 这类后缀），同一文件只能写一次。

> **触摸屏软键盘**：引擎内置软键盘（`loppe::SoftKeyboard`），焦点进 `input` 自动弹出、离开自动收起，
> 触摸不穿透、按键直达输入框，三平台同一实现，宿主零代码。DSL 无对应标签；细节见
> `../LOPPE_BUILTIN_CONTROLS.md` §4.11。

---

## 2. 选择器速查

| 语法 | 匹配 | 示例 |
|------|------|------|
| `type` | 控件类型名 | `panel`, `text`, `button` |
| `.class` | class 属性值 | `.header`, `.active` |
| `#id` | id 属性值 | `#root` |
| `:pseudo` | UI 状态 | `:hover`, `:active`, `:focus`, `:disabled` |
| 组合 | 顺序连接 | `panel.login-panel:hover` |

**限制**：不支持后代/子代组合器（不能写 `panel .title` 或 `panel > .title`）。一个选择器只匹配单个节点。

---

## 3. 节点语法

```
type                       // 最简
type:id                    // 带 id
type(attr=val, ...)        // 带属性
type:id(attr=val, ...)     // 完整形式
```

**属性列表语法**（括号内，逗号分隔）：
```
key=value                  // 单值
key="string"               // 字符串
key=[item1, item2]         // 数组
key.subkey=value           // ❌ 不支持嵌套
```

**子节点语法**（花括号内，逗号分隔）：
```
type(attr) { child1, child2, child3(id="x") }
```

**属性值自动类型推断**：

| Token 形式 | 推断为 |
|-----------|--------|
| `42` | int |
| `3.14`, `1e5` | float |
| `"hello"` | string |
| `true`, `false` | bool |
| `100px`, `50%`, `auto` | string（带单位的数字保留为字符串） |
| 其他标识符 | string |

字符串转义：`\n`、`\r`、`\t`、`\"`、`\\`。其中 `\t` 按制表位排版（显示行行首起每 `4 × font-size` 一格，
折行后的每一行按自己的行首对齐），占满到下一个制表位的那段空白（不画字形）；要表格状对齐就用 `\t`，不要用一串空格。

---

## 4. 单位体系

| 单位 | 含义 | 示例 |
|------|------|------|
| `px` | 像素 | `width: 100px` |
| `dp` | 密度无关像素（与 px 等价） | `width: 50dp` |
| `sp` | 缩放无关像素 | `font-size: 14sp` |
| `%` | 父容器百分比 | `width: 50%` |
| `auto` | 自动计算 | `width: auto` |
| 纯数字 | 默认视为 px | `width: 100` → 100px |

**颜色**：仅支持 `#RRGGBB`、`#RRGGBBAA` 或 `#RGB` 十六进制格式；`AA` 是 alpha 通道（`00` 全透明、`ff` 不透明），所有后端（桌面 GL / 安卓 / 嵌入式软光栅）都按「颜色 alpha × opacity」混合到下层内容。

---

## 5. 样式属性全表

### 5.1 颜色与背景

| 属性 | 类型 | 默认值 | 继承 | 说明 |
|------|------|--------|------|------|
| `background-color` | Color | `#00000000` (透明) | 否 | 背景色 |
| `color` | Color | `#000000` | **是** | 文字颜色 |
| `border-color` | Color | `#00000000` | 否 | 边框颜色 |
| `background-image` | string | `""` | 否 | 背景图片路径（`~/` = 本 .lui 文件所在目录） |

### 5.2 边框

| 属性 | 类型 | 默认值 | 继承 | 说明 |
|------|------|--------|------|------|
| `border` | shorthand | — | 否 | 简写：`"1px #ff0000"`（width color，忽略 solid 等关键字） |
| `border-width` | Edges | `0` | 否 | 四边统一宽度 |
| `border-width-top` | float | `0` | 否 | 上边框 |
| `border-width-right` | float | `0` | 否 | 右边框 |
| `border-width-bottom` | float | `0` | 否 | 下边框 |
| `border-width-left` | float | `0` | 否 | 左边框 |
| `border-radius` | px | `0` | 否 | 圆角 |

### 5.3 文字

| 属性 | 类型 | 默认值 | 继承 | 说明 |
|------|------|--------|------|------|
| `font-size` | px | `14` | **是** | 字号 |
| `font-family` | string | `""` | **是** | 字体名 |
| `font-weight` | keyword | `normal` | **是** | `normal` / `bold` |
| `line-height` | px | `0` | **是** | 行高，0=自动 |
| `text-align` | keyword | `left` | **是** | `left` / `center` / `right` / `justify` |

### 5.4 盒模型

| 属性 | 类型 | 默认值 | 继承 | 说明 |
|------|------|--------|------|------|
| `width` | Unit | `auto` | 否 | px/dp/sp/%/auto |
| `height` | Unit | `auto` | 否 | px/dp/sp/%/auto |
| `padding` | Edges | `0` | 否 | 四边内边距 |
| `padding-top/right/bottom/left` | px | `0` | 否 | 单边内边距 |
| `margin` | Edges | `0` | 否 | 四边外边距，支持 `auto` |
| `margin-top/right/bottom/left` | px | `0` | 否 | 单边外边距，支持 `auto` |
| `overflow` | keyword | `auto` | 否 | `visible` / `hidden` / `scroll` / `auto`（简写：两轴一起设） |
| `overflow-x` | keyword | 同 `overflow` | 否 | 只覆盖横向；`visible` = 不裁剪、不出横向条、横向溢出交给祖先滚动容器（与新建卡共用一条横向滚动条就靠它） |
| `overflow-y` | keyword | 同 `overflow` | 否 | 只覆盖纵向（用法同上） |
| `scrollbar-gutter` | keyword | 关闭 | 否 | **只认 `stable`**：滚动轴恒留一条槽（视口固定减一条条宽，有/无滚动条时内容区完全一致）。只对 `auto`/`scroll` 轴生效，两轴各自独立，槽在视口内侧；`overflow` 默认 `auto` → 不写 overflow 时两轴都留槽（宽高各减一条），只想留竖槽要写 `overflow-x: hidden`；`auto` / `stable both-edges` / 拼错的值 = 关闭。写 `@style` |

### 5.5 视觉效果

| 属性 | 类型 | 默认值 | 继承 | 说明 |
|------|------|--------|------|------|
| `opacity` | float | `1.0` | 否 | 不透明度 (0.0~1.0) |
| `visible` | bool | `true` | **是** | 可见性 |
| `box-shadow` | string | `""` | 否 | 外阴影：`[inset] <x> <y> [blur] [spread] <color>`，逗号分隔多条（先写的在上层）。只画在边框盒之外、跟着 `border-radius`；`inset` 暂不支持（忽略 + warning） |
| `overlay` | bool | `false` | 否 | 是否在叠加层渲染（脱离文档流） |
| `anchor` | keyword | `none` | 否 | `none` / `topleft` / `center` / `topright` / `bottomleft` / `bottomright`（不带连字符；`topleft` = 绝对坐标 + offset，其余按父内容盒 + margin） |
| `z-index` | int | `0` | 否 | 同级叠放顺序：大的在上；同值按声明顺序；负数沉到 0 的下面。同时决定命中顺序（建树时生效，运行期改 class 不重排） |

---

## 6. 布局属性全表

这些属性既可在 `@layout` 块中使用，也可内联在节点属性中。
**写错块 = 静默忽略**：布局属性写在 `@style` 里、`width`/`height`/`min-*`/`max-*` 写在 `@layout` 里，
都不会编译、不报错（见 §7.2 与用户指南 §7）。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `display` | keyword | `block` | `block` / `inline` / `flex` / `grid` / `none` |
| `position` | keyword | `static` | `static` / `relative` / `absolute` / `fixed` |
| `flex-direction` | keyword | `row` | `row` / `column` / `row-reverse` / `column-reverse` |
| `align-items` | keyword | `stretch` | `stretch` / `flex-start` / `flex-end` / `center` / `baseline` |
| `align-self` | keyword | `auto` | 子项级覆盖父 `align-items`：`auto` / `stretch` / `flex-start` / `flex-end` / `center` / `baseline` |
| `justify-content` | keyword | `flex-start` | `flex-start` / `flex-end` / `center` / `space-between` / `space-around` / `space-evenly` |
| `flex-wrap` | keyword | `nowrap` | `nowrap` / `wrap` / `wrap-reverse` |
| `flex-grow` | float | `0` | 弹性增长比例 |
| `flex-shrink` | float | `0` | 弹性收缩比例；仅显式启用时参与收缩 |
| `gap` | px | `0` | 子元素间距 |
| `grid-template-columns` | string | `""` | 网格列模板 |
| `grid-template-rows` | string | `""` | 网格行模板 |

---

## 7. Flex 实现细节与限制

### 7.1 已实现的特性

| 特性 | 状态 | 说明 |
|------|------|------|
| `flex-direction` | ✅ | row / column / row-reverse / column-reverse，反向正确 |
| `flex-wrap` | ✅ wrap/nowrap | wrap 换行正常；wrap-reverse 枚举存在但行堆叠顺序可能非标准 |
| `justify-content` | ✅ 全部 6 值 | flex-start / flex-end / center / space-between / space-around / space-evenly |
| `flex-grow` | ✅ | 按比例分配剩余空间，分配后触发子元素 remeasure |
| `flex-shrink` | ✅ | 按 shrink 值直接分配收缩量，达到 min-size 后冻结并重新分配 |
| `align-items: stretch` | ✅ | auto 尺寸可扩大；仅 `overflow != visible` 时可缩到容器交叉尺寸 |
| `align-items: center` | ✅ | 交叉轴居中 |
| `align-items: flex-end` | ✅ | 交叉轴末尾对齐 |
| `align-items: flex-start` | ✅ | 交叉轴起始（默认 fallthrough） |
| `align-self` | ✅ | 子项级覆盖父 `align-items`（auto=跟随父） |
| `gap` | ✅ | 子元素间距，参与换行和空间分配 |
| 收缩下限 | ✅ | 只使用显式 min-size；未设置时为 0，不计算 content-based 隐式下限 |
| 嵌套 remeasure | ✅ | grow/shrink/stretch 后自动重新测量后代 |

### 7.2 ⚠️ 未实现 / 非标准行为

| 缺失特性 | 影响 | 替代方案 |
|----------|------|---------|
| **`align-items: baseline`** | 枚举存在但无代码路径，退化为 flex-start | 固定高度 + center 近似 |
| **`flex-basis`** | 无显式初始主轴尺寸；`flex-grow` 的基准是**内容尺寸**，所以「等宽几栏」不能只靠 `flex-grow: 1` —— 某一栏内容一变化，那栏就变宽、其余被压缩 | 给确定尺寸：把 `width`（如 `width: 25%`）写在 **`@style`** 里配 `flex-grow`，或写固定 px |
| **`flex` 简写** | 不能 `flex: 1` | 分别写 `flex-grow` + `flex-shrink` |
| **`order`** | 无法改变视觉顺序 | 调整源码节点顺序 |
| **`align-content`** | 多行时交叉轴分布不可控 | 行仅简单堆叠 |
| **`row-gap / column-gap`** | 仅单一 `gap` | — |
| **百分比 gap** | gap 仅绝对长度 | — |
| **`wrap-reverse`** | 枚举存在但交叉轴可能非标准 | 优先用 wrap |
| **flex 子元素 `margin:auto`** | 不吸收剩余空间 | 仅普通盒模型行为 |

### 7.3 Grid 实现细节与限制

#### 已实现的特性

| 特性 | 状态 | 说明 |
|------|------|------|
| `grid-template-columns` | ✅ | px / % / fr / auto 轨道，空格分隔多列 |
| `grid-template-rows` | ✅ | 同上；缺省时行高按内容 |
| `gap` | ✅ | 统一行列间距，参与剩余空间计算 |
| 自动放置 | ✅ | 行优先（row-major）填充单元格 |
| 隐式行 | ✅ | 项超出行数自动补 auto 行 |
| stretch | ✅ | auto 尺寸项默认填满单元格（扣除自身 margin/border/padding） |
| `align-items` / `align-self` | ✅ | 块轴（垂直）对齐：stretch（默认）/ center / flex-end / flex-start；子项可用 `align-self` 覆盖 |

#### fr 分配规则

- 固定轨道（px / % / auto）先占据空间
- 剩余空间 = 容器内容尺寸 − 固定轨道合计 − gap 合计，按 `fr` 值比例分配给 fr 轨道
- 容器尺寸未知（auto）时 fr 轨道取 0，容器宽度由 px / auto 轨道决定

#### ⚠️ 未实现

| 缺失特性 | 影响 |
|----------|------|
| `grid-column` / `grid-row` | 无法手动定位/跨越，仅自动放置 |
| `minmax()` | 无法约束轨道最小/最大尺寸 |
| `grid-auto-rows` | 隐式行高固定为内容 |
| `grid-template-areas` | 无命名区域 |
| `justify-items` / `justify-content` / `align-content` | 单元格内水平对齐与网格整体对齐不可调（垂直方向可用 `align-items` / `align-self`） |

---

## 8. 布局算法行为

确定性规则：`width`/`height` 是 border-box 期望尺寸；min-size 默认 0，max-size 默认不限制；grow 和 shrink 相互独立；只有显式 `flex-shrink > 0` 的元素参与收缩；收缩量直接按 shrink 值分配，达到 min-size 后冻结并重新分配；无法继续收缩的空间成为 overflow。单行 flex 的 auto 交叉尺寸始终可 stretch 扩大；仅当子项 `overflow` 为 auto、scroll 或 hidden 时可缩到容器交叉尺寸，`overflow: visible` 则保留内容尺寸并把溢出交给父级。

| 布局模式 | display 值 | 行为 |
|---------|-----------|------|
| **Block** | `block`（默认） | 垂直堆叠。子元素宽度默认撑满父容器，高度由内容决定 |
| **Flex** | `flex` | 弹性布局（非标准 CSS Flexbox 子集）。详见第 7 节。不支持 baseline/flex-basis/order/align-content（align-self 已支持） |
| **Inline** | `inline` | 水平流式排列。子元素自动换行，类似文字流 |
| **Grid** | `grid` | 二维网格布局。支持 px/%/fr/auto 轨道、gap、自动放置、隐式行、stretch、align-items/align-self 块轴对齐（详见 7.3 节） |
| **隐藏** | `none` | 不参与布局和渲染 |

盒模型（由外向内）：**margin → border → padding → content**

`width`/`height`、`min-*` 和 `max-*` 始终表示 border box，包含 padding 和 border。

`overflow: scroll` 或 `auto` 的容器在内容溢出时提供滚动条和 `scrollWidth`/`scrollHeight`。

---

## 9. ⚠️ 与标准 CSS 的关键差异

| # | 场景 | ❌ 错误（CSS 习惯） | ✅ 正确（LOPPE DSL） |
|---|------|---------------------|---------------------|
| 1 | 后代选择器 | `.panel .title` | 为子节点单独设置 class，直接匹配 |
| 2 | 子代选择器 | `panel > text` | 同上，不支持组合器 |
| 3 | border 简写 | `border: 1px solid #ccc` | `border: 1px #cccccc`（solid 关键字被忽略） |
| 4 | em/rem 单位 | `font-size: 1.2em` | `font-size: 16px` 或 `font-size: 16sp` |
| 5 | vh/vw 单位 | `height: 100vh` | `height: 100%`（需父容器有明确高度） |
| 6 | flex 简写 | `flex: 1` | `flex-grow: 1; flex-shrink: 1` |
| 7 | flex-basis | `flex-basis: 200px` | ❌ 不支持；用 `width`/`height` 代替（写在 `@style`）；等宽几栏（如 tab 栏）必须给 `width`，只写 `flex-grow` 会随内容变宽 |
| 8 | align-self | `align-self: center` | ✅ 支持；flex/grid 子项上覆盖父 `align-items`（auto=跟随父） |
| 9 | order | `order: 1` | ❌ 不支持；调整节点源码顺序 |
| 10 | align-content | `align-content: space-between` | ❌ 不支持；多行仅简单堆叠 |
| 11 | align-items: baseline | `align-items: baseline` | ❌ 无效果，退化为 flex-start |
| 12 | position:absolute 坐标 | `position: absolute; top: 0; left: 0` | `overlay: true; anchor: topleft` |
| 13 | visibility/display:none | `visibility: hidden` | `visible: false` |
| 14 | display:none | `display: none` | `visible: false` 或 `display: none` |
| 15 | rgba() 颜色 | `rgba(0,0,0,0.5)` | `background-color: #00000080`（末两位 = alpha） |
| 16 | CSS 特异性计算 | 依赖选择器权重 | 规则按出现顺序覆盖（后面覆盖前面） |
| 17 | min/max-width/height | `min-width: 100px` | ✅ 轻量支持 `auto`、绝对长度和百分比 |
| 18 | 伪元素 | `::before`, `::after` | ❌ 不支持 |
| 19 | @media 查询 | `@media (max-width: 768px)` | ❌ 不支持 |
| 20 | transform/transition | `transform: rotate(45deg)` | ❌ 不支持 |
| 21 | z-index | `z-index: 10` | ✅ 同级兄弟之间生效（建树时定序；运行期改 class 不重排） |
| 22 | 颜色函数 | `rgb()`, `hsl()` | 仅 `#RRGGBB` / `#RRGGBBAA` / `#RGB`（例外：`box-shadow` 的字符串里还认 `rgb()` / `rgba()`） |
| 23 | border-style | `border-style: dashed` | ❌ 不支持（border 仅宽度+颜色） |
| 24 | background 简写 | `background: url(...) center` | `background-image: "path"`（仅图片路径） |
| 25 | flex 子项最小尺寸 | `min-width: 0`（CSS 里为让 flex 子项能缩到内容宽以下） | ❌ 不需要：无隐式内容最小尺寸，`min-width` 默认就是 `0` |
| 26 | scrollbar-gutter 取值 | `scrollbar-gutter: stable both-edges` | 只认 `stable`；`both-edges` 不支持（整串按关闭处理，且不额外留另一侧），槽恒在滚动轴内侧、写 `@style` |

---

## 10. 完整示例

### 10.1 登录面板

```loppe
@style {
    .container {
        width: 100%;
        height: 100%;
        background-color: #f8f8f8;
    }
    .card {
        width: 320px;
        margin: auto;
        padding: 32px;
        background-color: #ffffff;
        border-radius: 12px;
    }
    .title {
        font-size: 22px;
        color: #333333;
        margin-bottom: 24px;
    }
    .field {
        width: 100%;
        padding: 10px;
        margin-bottom: 16px;
        border: 1px #cccccc;
        border-radius: 6px;
    }
    .btn {
        width: 100%;
        padding: 12px;
        background-color: #007bff;
        color: #ffffff;
        font-size: 16px;
        border-radius: 6px;
    }
}

@layout {
    .card {
        display: flex;
        flex-direction: column;
        align-items: stretch;
        justify-content: center;
    }
}

panel(class="container") {
    panel(class="card") {
        text(class="title", value="用户登录"),
        input(class="field", placeholder="用户名"),
        input(class="field", placeholder="密码", type="password"),
        button(class="btn") { text(value="登录") }
    }
}
```

### 10.2 Flex 横向列表

```loppe
@style {
    .row { width: 100%; padding: 16px; }
    .item {
        width: 80px; height: 80px;
        background-color: #e9ecef;
        border-radius: 8px;
    }
    .item-label { font-size: 12px; color: #666666; margin-top: 4px; }
}

@layout {
    .row {
        display: flex;
        flex-direction: row;
        gap: 12px;
        align-items: center;
    }
    .item {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
    }
}

panel(class="row") {
    panel(class="item") { text(class="item-label", value="A") },
    panel(class="item") { text(class="item-label", value="B") },
    panel(class="item") { text(class="item-label", value="C") },
    panel(class="item") { text(class="item-label", value="D") }
}
```

### 10.3 滚动容器

```loppe
@style {
    .scroll-box {
        width: 300px; height: 200px;
        background-color: #ffffff;
        border: 1px #ced4da;
        border-radius: 4px;
        padding: 10px;
        overflow: scroll;
    }
    .row-item {
        width: 100%; height: 40px;
        margin-bottom: 8px;
        background-color: #e9ecef;
        border-radius: 4px;
        padding: 8px;
    }
}

panel(class="scroll-box") {
    panel(class="row-item") { text(value="Item 1") },
    panel(class="row-item") { text(value="Item 2") },
    panel(class="row-item") { text(value="Item 3") },
    panel(class="row-item") { text(value="Item 4") },
    panel(class="row-item") { text(value="Item 5") },
    panel(class="row-item") { text(value="Item 6") }
}
```

---

## 附录：宿主 C++ 代码的时机（AI 生成 C++ 必读）

生成「加载界面 + 往控件里填数据」的代码时，最容易错的是**时机**——错了不会报错，只会表现为「界面看着对、数据是空的」。

| 阶段 | 能做什么 | 不能做什么 |
|------|---------|-----------|
| `Scene::setup()` | 读 `.lui` → `notifyASTUpdated(ast)`；`setDocumentRoot`；按选择器绑事件 `onClick("#id")` / `onChange("#id")` / `onInput("#id")` | **取控件改属性**：这一刻控件还没建出来，`findControlById()` 静默返回 `nullptr` |
| `Scene::onControlsReady()` | 一切「改控件」的初始化：`setValue` / `setOptions` / `setSelectedIndex` / `setProgress` | 只能写一次（一帧可能多次回调 + 界面重解析会换控件实例 → 必须幂等） |

- `onControlsReady()` 在首帧 AST 同步之后、渲染之前回调；**主树重建会换控件实例**，所以每次回调都要重新取。
- `combobox` 的选项必须宿主 `setOptions({...})` 灌：`.lui` 的 `options="a,b"` 只给静态列表，逗号是分隔符。
  **选项为空时弹层高度 = 0**，用户看到的是「点了没反应」，不是控件坏了。
- `combobox` 的箭头是**引擎内置图标**（缺省 `chevron-down`，`arrow=""` 才不画），宿主不用注册；写别的图标名
  才需要 `svg::RegisterIcon`，否则那个位置什么都不画。
- 弹层是**覆盖层**（画在主树之上）：控件自己不能把它「压下去」，别试图用 `z-index` 解决。

---

## 附录：DSL 与 HTML+CSS 的关键差异（AI 生成必读）

以下差异是 DSL 设计层面的选择，与标准 CSS 行为不同，生成时必须注意。

### 1. ⭐ `overflow` 默认值为 `auto`（而非 CSS 的 `visible`）

标准 CSS 中块级元素默认 `overflow: visible`（内容溢出不会被裁剪）。
**LOPPE DSL 中默认 `overflow: auto`**，内容超出容器高度/宽度时自动出现滚动条。

```loppe
/* 未设置 overflow → 默认 auto */
.panel { height: 100px; }  
/* 内部子元素总高 > 100px 时 → 自动出现纵向滚动条 */
```

> 需要裁剪不显示滚动条：显式写 `overflow: hidden`
> 需要内容自由溢出：显式写 `overflow: visible`

⚠️ 因此**每个容器默认都是滚动容器**（包括 `display: flex` 的列表容器）：内容一长，
它就可能自己冒出一条滚动条 —— 哪怕父级宽度还装得下。若希望由外层统一滚动
（例如「整列 `.w-main-col` 是唯一滚动容器」），要把内层容器**显式**写成 `overflow: visible`
（它保留内容尺寸、把溢出交给父级），不能依赖默认值。

### 2. ⭐ LOPPE 始终使用 border-box

`width`、`height`、`min-*` 和 `max-*` 都包含 padding 与 border，不支持切换成 content-box。

```loppe
/* border box 高度精确等于父容器高度，padding 包含在其中 */
.card { height: 100%; padding: 24px; }
```

因此 `height: 100%` 与 padding 可以直接组合，不需要额外的 `box-sizing` 属性。

### 3. 固定尺寸容器需留意内容是否超出

因默认 `overflow: auto`，给容器写固定 `height` 或 `width` 时，若内部子元素总和超出该尺寸，会**自动出现滚动条**。不是 Bug，是设计行为。

```loppe
/* 若内容总高约 35px，固定 30px → 滚动条出现 */
.progress-area { height: 30px; }

/* 去掉固定高度或用 auto 让内容自然撑开 */
.progress-area { }
```

### 4. `background-image` 仅支持本地文件路径

不支持 HTTP URL。图片引擎通过本地文件系统加载。

```loppe
/* ❌ 不支持 */
background-image: "https://example.com/image.jpg";

/* ✅ 仅支持本地路径 */
background-image: "images/cover.png";
```

路径基准两种：`images/cover.png`（不带前缀）= **exe 所在目录**；`~/images/cover.png` = **本 .lui 文件所在目录**（`image` 的 `src` 同规则）。

### 5. 中文字体不含 emoji 字形

内置字体 `AlibabaPuHuiTi` 不含 emoji。使用 emoji（🔀 ⏮ ⏸ 等）会显示为方块 `□`。用英文文字代替。

```loppe
/* ❌ 不显示 */    text(value="🔀")
/* ✅ 可显示 */    text(value="RDM")
```

### 6. 多层嵌套时逐层核算 content-box 高度

```
Root (800×600)
  └── Container (100% × 100%)        = 800×600
       └── Card (height:100%) = 360×600
            └── content-box = 600 - 32padding = 568px
                 └── 子元素总高约 572px → 超出 4px → 出现滚动条
```

生成多层嵌套 DSL 时，逐层计算 content-box 是否能容纳子元素总和。

---

## 附录：继承属性清单

以下属性会从父节点自动继承（子节点不设置时使用父节点值）：

`color`, `font-size`, `font-family`, `font-weight`, `line-height`, `text-align`, `visible`
