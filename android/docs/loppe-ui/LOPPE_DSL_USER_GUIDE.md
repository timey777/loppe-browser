# LOPPE DSL 用户指南

> LOPPE（Layout Definition Template）是一种声明式 UI 描述语言，用于定义界面结构、样式和布局。

> 📖 中文 | [English](LOPPE_DSL_USER_GUIDE_EN.md)

---

## 1. 概述

一个 `.lui` 文件从上到下包含三部分：

- **@style 块**：定义界面元素的视觉样式（颜色、字体、边框、间距等）
- **@layout 块**：定义界面元素的布局方式（排列方向、对齐、弹性伸缩等）
- **节点树**：定义界面的控件层级结构（嵌套关系）

三者的关系是：节点树描述「有哪些控件，谁包含谁」，@style 描述「控件长什么样」，@layout 描述「控件如何排列」。

另外还可以在文件最前面写一行可选的 **`@version(1.0)` 版本声明**，声明本文件要求的
界面 DSL 版本，见 §2。

---

## 2. 文件结构

一个完整的 `.lui` 文件结构如下：

```loppe
@version(1.0)       /* 可选：本文件要求的界面 DSL 版本 */

@style {
    /* 样式规则 */
}

@layout {
    /* 布局规则 */
}

/* 节点树 */
```

`@style` 和 `@layout` 必须在节点树之前。如果不需要样式或布局，对应的块可以省略。

### 2.1 `@version(…)` 版本声明（可选）

- 写法**只有一种**：`@version(1.1.0)` —— `@version` 后跟括号，括号里是**裸版本号**（`x[.y[.z]]`，
  纯数字段，不带引号）；不写括号、把版本号写在括号外、或写成 `@version("1.1.0")` 一律报错。
  惯例写在**文件最前面**，引擎不强制位置，但同一文件**只能写一次**。
- 语义：声明本文件要求的**界面 DSL 版本**（类似 GLSL 的 `#version`）。解析时只比**主版本**：
  与引擎主版本一致 → 通过（同主版本下更高的小版本按兼容处理）；不一致 → 解析直接报错并说明当前引擎版本。
- 不写 = 不检查（按当前版本解析）—— 版本声明是可选的。
- 引擎版本来自构建：`build.lpj` 的 `lib loppe { version = x.y.z }` → `-DLOPPE_UI_VERSION`；
  运行时读 `loppe::engineVersion()`（见 `src/version.h`）。**引擎版本串本身可以带后缀**（如
  `1.0.0-preview.1`，发布预览版就是这么写的）；`loppe::isValidVersion()` / `compareVersions()` 容忍它
  （比较时忽略后缀），但 `.lui` 里写的 `@version(…)` 始终是纯数字段。

---

## 3. 选择器

选择器用于在 `@style` 和 `@layout` 块中指定规则应用于哪些节点。支持四种选择器类型：

### 3.1 类型选择器

直接使用控件类型名进行匹配：

```loppe
panel { ... }    /* 匹配所有 panel 控件 */
text { ... }     /* 匹配所有 text 控件 */
button { ... }   /* 匹配所有 button 控件 */
```

### 3.2 类选择器

以 `.` 开头，匹配节点的 `class` 属性：

```loppe
.header { ... }       /* 匹配 class 包含 "header" 的节点 */
.active { ... }       /* 匹配 class 包含 "active" 的节点 */
```

一个节点可以有多个 class（空格分隔），每个 class 都可以被独立匹配。

### 3.3 ID 选择器

以 `#` 开头，匹配节点的 `id` 属性：

```loppe
#root { ... }         /* 匹配 id 为 "root" 的节点 */
#submit-btn { ... }   /* 匹配 id 为 "submit-btn" 的节点 */
```

### 3.4 伪状态选择器

以 `:` 开头，匹配处于特定交互状态的节点：

| 伪状态 | 触发条件 |
|--------|---------|
| `:hover` | 鼠标悬停 |
| `:active` | 按下/激活状态 |
| `:focus` | 获得输入焦点 |
| `:disabled` | 禁用状态 |

```loppe
button:hover { background-color: #0056b3; }
input:focus { border-color: #007aff; }
```

### 3.5 组合使用

选择器可以组合多个条件：

```loppe
panel.login-panel { ... }      /* 类型 + 类 */
button.primary:hover { ... }   /* 类型 + 类 + 伪状态 */
```

---

## 4. 节点定义

### 4.1 基本语法

```
控件类型                         // 最简形式
控件类型:id                      // 指定标识符
控件类型(属性=值, ...)           // 设置属性
控件类型:id(属性=值, ...)        // 完整形式
```

**标识符（id）** 用 `:` 与类型分隔，如 `button:submitBtn`。id 在同一文件内应唯一，用于在代码中查找控件。

### 4.2 属性设置

属性在括号内以 `key=value` 形式给出，逗号分隔：

```loppe
text(value="Hello World", class="title")
input(placeholder="请输入", type="password")
panel(width=300, height=200, class="card")
```

**属性值支持以下类型：**

| 类型 | 写法 | 示例 |
|------|------|------|
| 整数 | 直接写数字 | `width=300` |
| 小数 | 带小数点 | `opacity=0.5` |
| 带单位的数值 | 数字+单位后缀 | `width=100px`, `width=50%` |
| 字符串 | 双引号包裹 | `value="Hello"` |
| 布尔值 | `true` 或 `false` | `visible=false` |
| 数组 | 方括号 | `items=[1,2,3]` |

字符串支持转义字符：`\n`（换行）、`\r`（回车）、`\t`（制表符）、`\"`（双引号）、`\\`（反斜杠）。

`\t` 在界面里按制表位排版：从行首起每 `4 × 字号` 一个制表位，`\t` 占满到下一个制表位的那段宽度（只占空白，不画字形）。光标、选区、命中都按这段宽度计算。文字折行后，每一段折出来的行都按它自己的行首重新对齐制表位。

```loppe
text(value="名称\t数量\t单价")
```

**特殊属性 `class`**：class 属性支持空格分隔多个类名：
```loppe
panel(class="card active highlight")
```

### 4.3 子节点

使用花括号 `{}` 包含子节点，子节点之间用逗号分隔：

```loppe
panel(class="container") {
    text(value="标题", class="title"),
    panel(class="body") {
        input(placeholder="输入内容"),
        button() { text(value="提交") }
    }
}
```

---

## 5. 单位系统

LOPPE DSL 支持以下五种单位：

| 单位 | 说明 | 适用场景 |
|------|------|---------|
| `px` | 像素 | 精确尺寸控制 |
| `dp` | 密度无关像素 | 跨屏幕密度一致的尺寸 |
| `sp` | 缩放无关像素 | 字体大小（可随系统缩放） |
| `%` | 百分比 | 相对父容器尺寸 |
| `auto` | 自动计算 | 根据内容或父容器自动决定 |

数值不写单位时默认视为 `px`（像素）。例如 `width=100` 等价于 `width=100px`。

**颜色值**使用十六进制格式：`#RRGGBB`（如 `#ff0000` 为红色）或 `#RGB`（如 `#f00` 也为红色）。支持透明度通道的格式为 `#RRGGBBAA`（如 `#00000080` 为 50% 透明黑）——透明部分与下层内容做 alpha 混合，所有平台（桌面 / 安卓 / 嵌入式）行为一致。

---

## 6. 样式属性

样式属性在 `@style` 块中使用，也可以内联在节点的括号属性中。

### 6.1 颜色

| 属性 | 说明 | 示例 |
|------|------|------|
| `background-color` | 背景颜色 | `background-color: #f0f0f0` |
| `color` | 文字颜色 | `color: #333333` |
| `border-color` | 边框颜色 | `border-color: #cccccc` |

### 6.2 边框

| 属性 | 说明 | 示例 |
|------|------|------|
| `border` | 边框简写（宽度+颜色） | `border: 1px #eeeeee` |
| `border-width` | 四边等宽设置 | `border-width: 2px` |
| `border-width-top` | 上边框宽度 | `border-width-top: 1px` |
| `border-width-right` | 右边框宽度 | `border-width-right: 2px` |
| `border-width-bottom` | 下边框宽度 | `border-width-bottom: 1px` |
| `border-width-left` | 左边框宽度 | `border-width-left: 2px` |
| `border-radius` | 圆角半径 | `border-radius: 8px` |

### 6.3 文字排版

| 属性 | 说明 | 可选值 | 示例 |
|------|------|--------|------|
| `font-size` | 字号 | 数值+单位 | `font-size: 16px` |
| `font-family` | 字体名称 | 字符串 | `font-family: "Arial"` |
| `font-weight` | 字重 | `normal`, `bold` | `font-weight: bold` |
| `line-height` | 行高 | 数值+单位 | `line-height: 24px` |
| `text-align` | 文字对齐 | `left`, `center`, `right`, `justify` | `text-align: center` |

以下属性支持**继承**：当子节点不设置时，自动使用父节点的值。这些属性包括 `color`、`font-size`、`font-family`、`font-weight`、`line-height`、`text-align`。

### 6.4 盒模型（尺寸与间距）

| 属性 | 说明 | 可选值 | 示例 |
|------|------|--------|------|
| `width` | 宽度 | 数值+单位/`auto` | `width: 300px` |
| `height` | 高度 | 数值+单位/`auto` | `height: 100%` |
| `min-width` | 最小宽度 | 数值+单位/`auto` | `min-width: 0` |
| `min-height` | 最小高度 | 数值+单位/`auto` | `min-height: 0` |
| `max-width` | 最大宽度 | 数值+单位/`auto` | `max-width: 600px` |
| `max-height` | 最大高度 | 数值+单位/`auto` | `max-height: 80%` |
| `padding` | 四边内边距 | 数值 | `padding: 16px` |
| `padding-top` | 上内边距 | 数值 | `padding-top: 8px` |
| `padding-right` | 右内边距 | 数值 | `padding-right: 12px` |
| `padding-bottom` | 下内边距 | 数值 | `padding-bottom: 8px` |
| `padding-left` | 左内边距 | 数值 | `padding-left: 12px` |
| `margin` | 四边外边距 | 数值/`auto` | `margin: 10px` |
| `margin-top` | 上外边距 | 数值/`auto` | `margin-top: auto` |
| `margin-right` | 右外边距 | 数值/`auto` | `margin-right: 20px` |
| `margin-bottom` | 下外边距 | 数值/`auto` | `margin-bottom: auto` |
| `margin-left` | 左外边距 | 数值/`auto` | `margin-left: 20px` |
| `overflow` | 溢出处理（两轴） | `visible`, `hidden`, `scroll`, `auto` | `overflow: scroll` |
| `overflow-x` | 横向溢出（只覆盖横向） | 同 `overflow` | `overflow-x: visible` |
| `overflow-y` | 纵向溢出（只覆盖纵向） | 同 `overflow` | `overflow-y: auto` |
| `scrollbar-gutter` | 滚动条恒占位（视口固定预留一条槽） | `stable` | `scrollbar-gutter: stable` |

`margin` 支持 `auto` 值，可用于水平居中（左右均设为 `auto`）。

`width`/`height`、`min-*` 和 `max-*` 始终表示 border box，已经包含 padding 和 border。

`overflow: scroll` 或 `auto` 会在内容超出容器时显示滚动条。

`overflow-x`/`overflow-y` 逐轴覆盖 `overflow`：某轴写 `visible` 时该轴不裁剪、不出滚动条，溢出交给最近的祖先滚动容器（所以「只纵向滚、横向跟父级共用一条滚动条」= `overflow-y: auto; overflow-x: visible`）。盒尺寸相关判定（flex 交叉轴 stretch）仍看 `overflow` 简写。

`scrollbar-gutter: stable`：**滚动轴恒定预留**一条滚动条宽，视口始终扣掉这条槽，与内容是否溢出、滚动条是否显示无关。所以「有滚动条」和「没滚动条」两种状态下内容区尺寸完全一致，子树不会重排跳动（列表上方有表头、又想让上下对齐时用它）。默认（不写）只在真出条时才占位。

- 只对 `overflow` 为 `auto` / `scroll` 的轴生效；`visible`（不裁剪）与 `hidden`（不出条）的轴不参与。
- **注意默认值**：`overflow` 默认是 `auto`，所以容器不写 `overflow` 时两轴都算可滚动 → 写 `stable` 会同时扣宽度和高度（各一条条宽）。只想给竖条留槽要显式写 `overflow-x: hidden`。
- X、Y 两轴各自独立：`overflow-x`/`overflow-y` 都可滚时两轴各留一条槽。
- 槽在**视口内侧**：节点声明的 `width`/`height` 不变，可用内容区减去一条条宽。
- 槽宽 = 本节点的 `scrollbar-width`，未写时用全局默认粗细（8dp）。
- 写在 `@style` 块（与 `scrollbar-width` 同处）；写进 `@layout` 会告警且不生效。
- 与标准 CSS 的差异：**只认 `stable`**。写 `auto`（或其它任何字符串）= 关闭；`stable both-edges` 不支持，整串不等于 `stable`，按关闭处理（不会告警）。

尺寸约束支持 `auto`、绝对长度和百分比。未设置的 min-size 为 0，未设置的 max-size 不限制；不存在基于内容的隐式最小尺寸（因此不需要像 CSS 那样给 flex 子项补 `min-width: 0`）。

### 6.5 视觉效果

| 属性 | 说明 | 示例 |
|------|------|------|
| `opacity` | 不透明度（0.0=全透明，1.0=不透明） | `opacity: 0.8` |
| `visible` | 是否可见 | `visible: false` |
| `box-shadow` | 外阴影（CSS 写法，支持多重阴影） | `box-shadow: "0 2px 8px #00000040"` |
| `background-image` | 背景图片路径 | `background-image: "images/bg.png"` |

**`box-shadow` 语法**：`[inset] <x偏移> <y偏移> [模糊] [扩散] <颜色>`（长度单位 px 或纯数值，颜色可写在最前），
多条阴影用逗号分隔，**先写的在上层**：

```
box-shadow: "0 2px 8px #00000040";                       /* 下偏移 2px、模糊 8px、25% 黑 */
box-shadow: "0 1px 2px #0000001f, 0 4px 12px #00000026"; /* 双层：贴边硬影 + 大范围柔影 */
box-shadow: "rgba(0, 0, 0, 0.25) 0 0 12px 2px";          /* 颜色在前 + 扩散 2px */
```

颜色支持 `#RGB` / `#RRGGBB` / `#RRGGBBAA` / `rgb()` / `rgba()` / `transparent`；`none` 或不写 = 无阴影。
与 CSS 一致的语义：阴影只画在**边框盒之外**（盒内不铺阴影），模糊从边框向外衰减到 `模糊` 距离为止，
阴影跟着 `border-radius` 走（圆角同步外扩）。**`inset`（内阴影）暂不支持**，写了会被忽略并在日志里告警。
阴影画在控件自己的边框与内容之下（同级里仍按声明顺序叠放），所以要盖住别的内容仍靠 `z-index`。

**渲染质量（宿主 C++ 侧配置，进程级全局）**：`loppe::setBoxShadowQuality()`，档位
`Gaussian`（默认，GL 端解析高斯：连续渐变、无条带）/ `High` / `Medium` / `Low`（环逼近，16/8/4 环，
嵌入式等无 GPU 端用，档位越低越省性能）/ `None`（不画）。`.lui` 语法不感知档位，同一份样式三端通用。

**图片路径（`background-image` 与 `image` 的 `src`）**：都是本地文件路径，两种基准：

| 写法 | 相对谁 |
|------|--------|
| `images/bg.png` | **exe 所在目录**（找不到再试 `<exe目录>/resources/images/bg.png`） |
| `~/images/bg.png` | **本 .lui 文件所在目录**（资源跟 .lui 一起走时用这个） |

### 6.6 叠加层与定位

| 属性 | 说明 | 可选值 | 示例 |
|------|------|--------|------|
| `overlay` | 是否在叠加层渲染（脱离正常文档流） | `true`, `false` | `overlay: true` |
| `anchor` | 叠加层锚点位置 | `none`, `topleft`, `center`, `topright`, `bottomleft`, `bottomright` | `anchor: center` |
| `z-index` | 同级叠放顺序 | 整数，默认 `0` | `z-index: 100` |

`overlay: true` 使节点脱离正常的布局流，显示在其他内容之上。`anchor` 决定它贴在哪：`center` 与三个角
（`topright`/`bottomleft`/`bottomright`）都按**父容器内容盒**定位，`margin` 就是离该边的内缩；
`topleft` 例外——它是「窗口原点 + `offset`」的绝对坐标（弹层落位靠它按屏幕坐标写 `offset`）。
值的拼写**不带连字符**（写 `top-left` 会解析成 `none`，静默不定位）。

`z-index` 管的是**同一父节点下的兄弟**谁画在上面：数值大的在上；数值相同按声明顺序（后声明的在上）；
负数排在默认 `0` 的兄弟前面。它同时决定命中测试的顺序（最上面的先响应点击），所以「挡点击的层」
和「看得见的层」永远一致。`z-index` 参与初始建树，运行期切 class 改它不会重排叠放（需要重建树）。

---

## 7. 布局属性

布局属性在 `@layout` 块中使用，也可以内联在节点的括号属性中。

> ⚠️ **块不能写错**：布局属性只在 `@layout` 块编译，写在 `@style` 块里会被**静默忽略**
> （没有报错、没有警告，只有界面走样）；反过来，`width` / `height` / `max-height` / `min-*`
> 是**尺寸属性**，只在 `@style` 块（或内联）编译，写在 `@layout` 里同样被静默忽略。
> 记忆法：`@layout` 管「怎么排列」（§7 这张表），其余（含尺寸）放 `@style`。

| 属性 | 说明 | 可选值 | 示例 |
|------|------|--------|------|
| `display` | 布局模式 | `block`, `inline`, `flex`, `grid`, `none` | `display: flex` |
| `position` | 定位方式 | `static`, `relative`, `absolute`, `fixed` | `position: absolute` |

### 7.1 Flex 布局属性

当 `display` 设为 `flex` 时，以下属性控制弹性布局行为：

| 属性 | 说明 | 可选值 | 默认值 |
|------|------|--------|--------|
| `flex-direction` | 主轴方向 | `row`, `column`, `row-reverse`, `column-reverse` | `row` |
| `align-items` | 交叉轴对齐 | `stretch`, `flex-start`, `flex-end`, `center`, `baseline` | `stretch` |
| `align-self` | 子项级交叉轴对齐（覆盖父 `align-items`） | `auto`, `stretch`, `flex-start`, `flex-end`, `center`, `baseline` | `auto` |
| `justify-content` | 主轴对齐 | `flex-start`, `flex-end`, `center`, `space-between`, `space-around`, `space-evenly` | `flex-start` |
| `flex-wrap` | 是否换行 | `nowrap`, `wrap`, `wrap-reverse` | `nowrap` |
| `flex-grow` | 弹性增长比例 | 数值 | `0` |
| `flex-shrink` | 弹性收缩比例 | 数值 | `0` |
| `gap` | 子元素间距 | 数值+单位 | `0` |

### 7.2 Grid 布局属性

当 `display` 设为 `grid` 时：

| 属性 | 说明 | 示例 |
|------|------|------|
| `grid-template-columns` | 列模板 | `grid-template-columns: "1fr 2fr 1fr"` |
| `grid-template-rows` | 行模板 | `grid-template-rows: "auto 1fr auto"` |
| `align-items` | 块轴（垂直）对齐 | `align-items: center` |
| `align-self` | 子项级块轴对齐（覆盖父 `align-items`） | `align-self: flex-end` |

---

## 8. 布局模型

尺寸、边界、伸缩与溢出的完整确定性规则见 [LAYOUT_DESIGN.md](LAYOUT_DESIGN.md)。

### 8.1 Block 布局（默认）

`display: block` 是默认的布局模式。特性：

- 子元素**垂直堆叠**，一个接一个向下排列
- 子元素宽度默认**撑满父容器**的内容宽度
- 高度由子元素内容决定

适用于页面整体结构、卡片列表等自上而下的排列场景。

### 8.2 Flex 布局

`display: flex` 启用弹性布局。**注意**：LOPPE 的 Flex 实现是一个精选特性子集，包含最常用的 Flex 功能，但并非完整的 CSS Flexbox 标准实现。

#### 已支持的特性

**方向控制：**
- 四种主轴方向：`row`（水平从左到右）、`column`（垂直从上到下）、`row-reverse`、`column-reverse`
- 反向布局正确渲染

**换行：**
- `nowrap`（默认）：单行排列，子元素可能溢出容器
- `wrap`：超出容器宽度/高度时自动换到下一行/列

**主轴对齐（justify-content）— 全部 6 种模式：**
- `flex-start` — 起始端对齐
- `flex-end` — 末尾端对齐
- `center` — 居中对齐
- `space-between` — 两端对齐，中间均匀分布
- `space-around` — 每个子元素两侧有相等间距
- `space-evenly` — 所有间距（含首尾）完全相等

**交叉轴对齐（align-items）— 4 种模式：**
- `stretch`（默认）— auto 尺寸可拉伸；仅 `overflow` 非 `visible` 时允许缩到容器交叉尺寸
- `flex-start` — 起始端对齐
- `flex-end` — 末尾端对齐
- `center` — 居中对齐

**子项级覆盖（align-self）：**
- 写在子元素上，覆盖父容器的 `align-items`；`auto`（默认）表示跟随父容器
- 可选值：`stretch` / `flex-start` / `flex-end` / `center`（`baseline` 无效果）

**弹性伸缩：**
- `flex-grow`：弹性增长比例。容器有剩余空间时，按 grow 值比例分配给子元素。默认 0（不增长）
- `flex-shrink`：弹性收缩比例。容器空间不足时，按 shrink 值直接分配收缩量。默认 0（不收缩）
- 只有显式设置 `flex-shrink > 0` 的元素参与收缩；达到显式 min-size 后冻结并重新分配剩余收缩量
- 伸缩发生后自动重新测量受影响的子元素及其后代

**间距：**
- `gap`：统一设置子元素之间的间距

#### 不支持的特性

以下 Flex 特性**暂未实现**：

| 不支持的特性 | 说明 |
|-------------|------|
| `align-items: baseline` | 无法按文字基线对齐（枚举值存在但实际无效果） |
| `flex-basis` | 无独立的初始主轴尺寸属性，子元素尺寸仅由 `width`/`height` 决定 |
| `flex` 简写属性 | 不能写 `flex: 1`，需要分别写 `flex-grow` 和 `flex-shrink` |
| `order` | 无法改变子元素的视觉排列顺序，顺序由源码位置决定 |
| `align-content` | 多行时行与行之间的交叉轴分布不可控，仅简单堆叠 |

适用于工具栏、导航栏、卡片网格等需要灵活对齐的场景。

### 8.3 Grid 布局

`display: grid` 启用二维网格布局。**注意**：LOPPE 的 Grid 是最小实现——行和列同时定义，子元素按源码顺序自动填入单元格，不实现完整的 CSS Grid 标准。

#### 轨道模板

`grid-template-columns` 定义列轨道，`grid-template-rows` 定义行轨道，多个值用空格分隔：

| 轨道值 | 说明 |
|--------|------|
| `100px` | 固定宽度 |
| `50%` | 相对容器内容宽度 |
| `1fr` | 弹性轨道：按比例分配剩余空间（如 `1fr 2fr` 表示第二列是第一列的 2 倍） |
| `auto` | 由该轨道上内容最宽的项决定 |

不写 `grid-template-rows` 时，行高由内容决定。

#### 行为

- **自动放置**：子元素从左到右、从上到下填入单元格，占满一列自动换行
- **隐式行**：子元素数量超过定义行数时自动创建新行，行高按内容
- **stretch**：`width`/`height` 为 auto 的项默认拉伸填满所在单元格（扣除自身 margin/border/padding）
- **align-items / align-self**：块轴（垂直）对齐。默认 `stretch`（拉满行高）；`center` / `flex-end` / `flex-start` 时子项保留内容高度并在行内对齐，子项可写 `align-self` 单独覆盖
- **gap**：统一设置行与列之间的间距，参与剩余空间计算

#### 示例

```loppe
@style {
    .grid { width: 100%; height: 100%; }
}
@layout {
    .grid { display: grid; grid-template-columns: 200px 1fr 1fr; gap: 8px; }
}
panel:grid(class="grid") {
    panel:sidebar(class="sidebar"),
    panel:item1(class="card"),
    panel:item2(class="card")
}
```

#### 限制

| 不支持的特性 | 说明 |
|-------------|------|
| `grid-column` / `grid-row` | 无法手动指定位置或跨越，仅自动放置 |
| `grid-template-areas` | 不支持命名区域 |
| `minmax()` | 不支持轨道最小/最大尺寸 |
| `grid-auto-rows` | 隐式行高固定按内容 |
| `justify-items` / `justify-content` / `align-content` | 单元格内水平对齐与网格整体对齐不可调（垂直方向可用 `align-items` / `align-self`） |

### 8.4 Inline 布局

`display: inline` 启用行内布局。特性：

- 子元素**水平排列**，类似文字流
- 超出容器宽度时**自动换行**
- 每个子元素保持自身固有宽度

适用于标签组、文字+图标混合排列等场景。

### 8.5 盒模型

每个控件都是一个矩形盒子，从外到内分为四层：

```
┌─────────────────────────────┐
│         margin              │  ← 外边距（与其他控件的间距）
│  ┌───────────────────────┐  │
│  │       border          │  │  ← 边框
│  │  ┌─────────────────┐  │  │
│  │  │    padding      │  │  │  ← 内边距（边框与内容之间的留白）
│  │  │  ┌───────────┐  │  │  │
│  │  │  │  content  │  │  │  │  ← 内容区（文字、图片等实际内容）
│  │  │  └───────────┘  │  │  │
│  │  └─────────────────┘  │  │
│  └───────────────────────┘  │
└─────────────────────────────┘
```

- `width`/`height` 始终控制 border box，padding 和 border 包含在声明尺寸内

---

## 9. 完整示例

### 9.1 简单登录面板

```loppe
@style {
    .page {
        width: 100%;
        height: 100%;
        background-color: #f5f5f5;
    }
    .card {
        width: 320px;
        margin: auto;
        padding: 40px 32px;
        background-color: #ffffff;
        border-radius: 12px;
        box-shadow: "0 2px 12px #00000020";
    }
    .title {
        font-size: 24px;
        font-weight: bold;
        color: #333333;
        text-align: center;
        margin-bottom: 32px;
    }
    .input-field {
        width: 100%;
        padding: 12px;
        margin-bottom: 16px;
        border: 1px #dddddd;
        border-radius: 8px;
        font-size: 14px;
    }
    .login-btn {
        width: 100%;
        padding: 14px;
        background-color: #007aff;
        color: #ffffff;
        font-size: 16px;
        font-weight: bold;
        border-radius: 8px;
    }
}

@layout {
    .page {
        display: flex;
        align-items: center;
        justify-content: center;
    }
    .card {
        display: flex;
        flex-direction: column;
        align-items: stretch;
    }
}

panel(class="page") {
    panel(class="card") {
        text(class="title", value="账号登录"),
        input:username(class="input-field", placeholder="请输入用户名"),
        input:password(class="input-field", placeholder="请输入密码", type="password"),
        button:loginBtn(class="login-btn") { text(value="登 录") }
    }
}
```

### 9.2 横向商品列表

```loppe
@style {
    .list-container {
        width: 100%;
        padding: 16px;
        background-color: #ffffff;
    }
    .product-card {
        width: 120px;
        padding: 12px;
        background-color: #f8f9fa;
        border-radius: 8px;
    }
    .product-image {
        width: 100px;
        height: 100px;
        background-color: #e9ecef;
        border-radius: 6px;
    }
    .product-name {
        font-size: 13px;
        color: #333333;
        margin-top: 8px;
    }
    .product-price {
        font-size: 15px;
        font-weight: bold;
        color: #ff3b30;
        margin-top: 4px;
    }
}

@layout {
    .list-container {
        display: flex;
        flex-direction: row;
        gap: 12px;
        overflow: scroll;
    }
    .product-card {
        display: flex;
        flex-direction: column;
        align-items: center;
    }
}

panel(class="list-container") {
    panel(class="product-card") {
        image(class="product-image", src="p1.png"),
        text(class="product-name", value="商品名称"),
        text(class="product-price", value="¥99")
    },
    panel(class="product-card") {
        image(class="product-image", src="p2.png"),
        text(class="product-name", value="商品名称"),
        text(class="product-price", value="¥199")
    },
    panel(class="product-card") {
        image(class="product-image", src="p3.png"),
        text(class="product-name", value="商品名称"),
        text(class="product-price", value="¥299")
    }
}
```

### 9.3 带滚动区域的页面框架

```loppe
@style {
    .app {
        width: 100%;
        height: 100%;
        background-color: #f2f2f7;
    }
    .navbar {
        width: 100%;
        height: 56px;
        padding: 0 16px;
        background-color: #ffffff;
        border-bottom-width: 1px;
        border-color: #e5e5ea;
    }
    .navbar-title {
        font-size: 18px;
        font-weight: bold;
        color: #000000;
    }
    .content {
        width: 100%;
        padding: 16px;
        overflow: auto;
    }
    .tabbar {
        width: 100%;
        height: 50px;
        background-color: #ffffff;
        border-top-width: 1px;
        border-color: #e5e5ea;
    }
    .tab-item {
        font-size: 11px;
        color: #8e8e93;
    }
    .tab-item.active {
        color: #007aff;
    }
}

@layout {
    .app {
        display: flex;
        flex-direction: column;
    }
    .navbar {
        display: flex;
        flex-direction: row;
        align-items: center;
    }
    .content {
        flex-grow: 1;
        flex-shrink: 1;
    }
    .tabbar {
        display: flex;
        flex-direction: row;
        justify-content: space-around;
        align-items: center;
    }
    .tab-item {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
    }
}

panel(class="app") {
    panel(class="navbar") {
        text(class="navbar-title", value="应用标题")
    },
    panel(class="content") {
        /* 可滚动的内容区域 */
        text(value="这里是可滚动的内容...")
    },
    panel(class="tabbar") {
        panel(class="tab-item active") { text(value="首页") },
        panel(class="tab-item") { text(value="发现") },
        panel(class="tab-item") { text(value="消息") },
        panel(class="tab-item") { text(value="我的") }
    }
}
```

---

## 10. 常用控件类型

| 类型 | 说明 | 常用属性 |
|------|------|---------|
| `panel` | 容器/面板 | 通用样式和布局属性 |
| `text` | 文本标签 | `value`（文本内容） |
| `button` | 按钮 | 通过子 `text` 节点设置文字 |
| `input` | 输入框 | `placeholder`（占位文字）, `type`（类型） |
| `combobox` | 下拉框 | `options`（逗号分隔的选项）, `arrow`（展开箭头用的图标名，缺省 `chevron-down`，空串 = 不画） |
| `progress` | 进度条 | `value`（0～1 的声明取值）；轨道 = 自身背景（`background-color` / `border-radius`） |
| `icon` | 图标 | `name`（已注册的图标名，见 `SvgRegistry`） |
| `image` | 图片 | `src`（图片路径；`~/` = 本 .lui 目录） |
| `radio` | 单选 | 见 `src/components/radio*` |
| `scrollbar` | 滚动条 | 由引擎自动管理，一般不需要手动创建 |

> `combobox` 的箭头是**引擎内置图标**（缺省 `chevron-down`，另有 `chevron-up` / `chevron-right` / `chevron-left`
> 可写进 `arrow`），不需要宿主注册；`arrow=""` 不画箭头，运行期用 `setArrowIcon` 换。
> 想换画法或指向自己的图标名，才用 `svg::RegisterIcon` 注册（同名覆盖；没注册的名字不画）。

**运行期更新（引擎控件 API，UI 线程；跨线程先 `UiDispatcher::post`）**

| 控件 | API | 说明 |
|------|-----|------|
| `text` | `setText` | 改文字（写回 AST 由宿主决定） |
| `combobox` | `setOptions`（**替换**，可反复调用刷新）/ `setSelectedIndex` / `getSelectedIndex` / `setPlaceholder` / `setOnWillOpen`（开前刷新选项）/ `setArrowIcon`（改展开箭头图标） | 选中变化经 `UIEventSystem` 派发 `Change` 事件 → 宿主用 `scene->onChange("#id", cb)` 监听（`ev.text` = 选中项文本） |
| `progress` | `setProgress(0..1)` / `setBarColor` | 填充由控件自绘；**不要再在 `Scene::render()` 里手绘进度条**。描述文件里写 `value` 可给出声明取值（不写则由宿主设置） |
| `input` | `setValue` / `focus()` | 聚焦后光标闪烁由控件自己驱动（见下） |

> 光标闪烁这类「按时间推进、外观只在少数帧变化」的控件：`update()` 里用
> `requestAnimationTick()` 续命，**只有真的变了才 `invalidate()`** —— `invalidate()` 会请求重绘，
> 每帧调它等于每帧整屏重绘（CPU/GPU 白烧；见 `knowledge/repo-memory/caret-blink-repaint.md`）。

---

## 附录：可继承属性

以下样式属性在子节点不设置时会自动从父节点继承：

- `color` — 文字颜色
- `font-size` — 字号
- `font-family` — 字体
- `font-weight` — 字重
- `line-height` — 行高
- `text-align` — 文字对齐
- `visible` — 可见性

这意味着如果你在父容器上设置了 `font-size: 16px`，所有子节点的文字默认都是 16px，除非子节点显式覆盖。
