# LOPPE DSL User Guide

> LOPPE (Layout Definition Template) is a declarative UI description language for defining interface structure, styling, and layout.

> 📖 [中文版](LOPPE_DSL_USER_GUIDE.md) | English

---

## 1. Overview

An `.lui` file consists of three sections from top to bottom:

- **@style block**: Defines visual styles for UI elements (color, font, border, spacing, etc.)
- **@layout block**: Defines layout behavior for UI elements (direction, alignment, flex, etc.)
- **Node tree**: Defines the widget hierarchy (nesting relationships)

The relationship: the node tree describes **what widgets exist and who contains whom**, @style describes **what widgets look like**, and @layout describes **how widgets are arranged**.

You may also put an optional **`@version(1.0)` declaration** at the very top of the file to state
the UI DSL version it requires — see §2.

---

## 2. File Structure

A complete `.lui` file structure:

```loppe
@version(1.0)       /* Optional: UI DSL version required by this file */

@style {
    /* Style rules */
}

@layout {
    /* Layout rules */
}

/* Node tree */
```

`@style` and `@layout` must appear before the node tree. If no styles or layouts are needed, the corresponding blocks can be omitted.

### 2.1 `@version(…)` Declaration (Optional)

- There is exactly **one** spelling: `@version(1.1.0)` — `@version` followed by parentheses holding a **bare**
  version number (`x[.y[.z]]`, plain numeric segments, no quotes). Omitting the parentheses, putting the number
  outside them, or writing
  `@version("1.1.0")` is an error. Conventionally the first line of the file; the engine does not enforce the
  position, but the directive may appear **only once** per file.
- Semantics: declares the **UI DSL version** this file requires (the equivalent of GLSL's `#version`). Only the
  **major** version is compared: same major as the engine → accepted (a higher minor/patch within the same major is
  treated as compatible); different major → parsing fails immediately with the current engine version in the message.
- Omitted = no check (parsed as the current version) — the declaration is optional.
- The engine version comes from the build: `lib loppe { version = x.y.z }` in `build.lpj` →
  `-DLOPPE_UI_VERSION`; read it at runtime via `loppe::engineVersion()` (see `src/version.h`). **The engine version
  string itself may carry a suffix** (e.g. `1.0.0-preview.1`, as preview releases are built); `loppe::isValidVersion()`
  and `compareVersions()` tolerate it (the suffix is ignored when comparing), but what you write in `@version(…)`
  inside a `.lui` file is always plain numeric segments.

---

## 3. Selectors

Selectors specify which nodes a rule in `@style` or `@layout` applies to. Four selector types are supported:

### 3.1 Type Selector

Matches by widget type name:

```loppe
panel { ... }    /* Matches all panel widgets */
text { ... }     /* Matches all text widgets */
button { ... }   /* Matches all button widgets */
```

### 3.2 Class Selector

Begins with `.`, matches the node's `class` attribute:

```loppe
.header { ... }       /* Matches nodes whose class includes "header" */
.active { ... }       /* Matches nodes whose class includes "active" */
```

A node can have multiple classes (space-separated), each independently matchable.

### 3.3 ID Selector

Begins with `#`, matches the node's `id` attribute:

```loppe
#root { ... }         /* Matches the node with id "root" */
#submit-btn { ... }   /* Matches the node with id "submit-btn" */
```

### 3.4 Pseudo-State Selector

Begins with `:`, matches nodes in specific interaction states:

| Pseudo-state | Trigger Condition |
|--------------|-------------------|
| `:hover` | Mouse hover |
| `:active` | Pressed / active state |
| `:focus` | Input focus |
| `:disabled` | Disabled state |

```loppe
button:hover { background-color: #0056b3; }
input:focus { border-color: #007aff; }
```

### 3.5 Combining Selectors

Selectors can combine multiple conditions:

```loppe
panel.login-panel { ... }      /* Type + Class */
button.primary:hover { ... }   /* Type + Class + Pseudo-state */
```

---

## 4. Node Definition

### 4.1 Basic Syntax

```
widgetType                         // Simplest form
widgetType:id                      // With identifier
widgetType(attr=value, ...)        // With attributes
widgetType:id(attr=value, ...)     // Full form
```

**Identifier (id)** is separated from type by `:`, e.g. `button:submitBtn`. IDs should be unique within a file, used for looking up widgets in code.

### 4.2 Attribute Settings

Attributes are given in parentheses as `key=value`, comma-separated:

```loppe
text(value="Hello World", class="title")
input(placeholder="Enter text", type="password")
panel(width=300, height=200, class="card")
```

**Supported attribute value types:**

| Type | Syntax | Example |
|------|--------|---------|
| Integer | Plain number | `width=300` |
| Float | With decimal point | `opacity=0.5` |
| Unit value | Number + unit suffix | `width=100px`, `width=50%` |
| String | Double-quoted | `value="Hello"` |
| Boolean | `true` or `false` | `visible=false` |
| Array | Square brackets | `items=[1,2,3]` |

Strings support escape characters: `\n` (newline), `\r` (carriage return), `\t` (tab), `\"` (double quote), `\\` (backslash).

`\t` is laid out on tab stops: one stop every `4 × font size` from the start of the row. A `\t` fills the gap up to the next stop and draws nothing. Caret, selection and hit testing all use that width. When text wraps, each wrapped row re-aligns its tab stops to its own row start.

```loppe
text(value="Name\tQty\tPrice")
```

**Special attribute `class`**: The class attribute supports space-separated multiple class names:
```loppe
panel(class="card active highlight")
```

### 4.3 Child Nodes

Use curly braces `{}` to contain child nodes, separated by commas:

```loppe
panel(class="container") {
    text(value="Title", class="title"),
    panel(class="body") {
        input(placeholder="Enter content"),
        button() { text(value="Submit") }
    }
}
```

---

## 5. Unit System

LOPPE DSL supports five unit types:

| Unit | Description | Use Case |
|------|-------------|----------|
| `px` | Pixels | Precise size control |
| `dp` | Density-independent pixels | Consistent sizing across screen densities |
| `sp` | Scale-independent pixels | Font sizes (scales with system settings) |
| `%` | Percentage | Relative to parent container size |
| `auto` | Automatic | Auto-calculated based on content or parent |

Numeric values without a unit default to `px` (pixels). For example, `width=100` is equivalent to `width=100px`.

**Color values** use hexadecimal format: `#RRGGBB` (e.g. `#ff0000` for red) or `#RGB` (e.g. `#f00` also red). Alpha channel is supported as `#RRGGBBAA` (e.g. `#00000080` for 50% transparent black) — the transparent part is alpha-blended over the content beneath, consistently on all platforms (desktop / Android / embedded).

---

## 6. Style Properties

Style properties are used in the `@style` block and can also be inlined in node attribute parentheses.

### 6.1 Colors

| Property | Description | Example |
|----------|-------------|---------|
| `background-color` | Background color | `background-color: #f0f0f0` |
| `color` | Text color | `color: #333333` |
| `border-color` | Border color | `border-color: #cccccc` |

### 6.2 Borders

| Property | Description | Example |
|----------|-------------|---------|
| `border` | Border shorthand (width + color) | `border: 1px #eeeeee` |
| `border-width` | Uniform border width | `border-width: 2px` |
| `border-width-top` | Top border width | `border-width-top: 1px` |
| `border-width-right` | Right border width | `border-width-right: 2px` |
| `border-width-bottom` | Bottom border width | `border-width-bottom: 1px` |
| `border-width-left` | Left border width | `border-width-left: 2px` |
| `border-radius` | Border radius | `border-radius: 8px` |

### 6.3 Typography

| Property | Description | Accepted Values | Example |
|----------|-------------|-----------------|---------|
| `font-size` | Font size | Value + unit | `font-size: 16px` |
| `font-family` | Font name | String | `font-family: "Arial"` |
| `font-weight` | Font weight | `normal`, `bold` | `font-weight: bold` |
| `line-height` | Line height | Value + unit | `line-height: 24px` |
| `text-align` | Text alignment | `left`, `center`, `right`, `justify` | `text-align: center` |

The following properties support **inheritance**: when a child node does not set them, it automatically uses the parent's value. These include `color`, `font-size`, `font-family`, `font-weight`, `line-height`, and `text-align`.

### 6.4 Box Model (Size & Spacing)

| Property | Description | Accepted Values | Example |
|----------|-------------|-----------------|---------|
| `width` | Width | Value + unit / `auto` | `width: 300px` |
| `height` | Height | Value + unit / `auto` | `height: 100%` |
| `min-width` | Minimum width | Value + unit / `auto` | `min-width: 0` |
| `min-height` | Minimum height | Value + unit / `auto` | `min-height: 0` |
| `max-width` | Maximum width | Value + unit / `auto` | `max-width: 600px` |
| `max-height` | Maximum height | Value + unit / `auto` | `max-height: 80%` |
| `padding` | Uniform padding | Value | `padding: 16px` |
| `padding-top` | Top padding | Value | `padding-top: 8px` |
| `padding-right` | Right padding | Value | `padding-right: 12px` |
| `padding-bottom` | Bottom padding | Value | `padding-bottom: 8px` |
| `padding-left` | Left padding | Value | `padding-left: 12px` |
| `margin` | Uniform margin | Value / `auto` | `margin: 10px` |
| `margin-top` | Top margin | Value / `auto` | `margin-top: auto` |
| `margin-right` | Right margin | Value / `auto` | `margin-right: 20px` |
| `margin-bottom` | Bottom margin | Value / `auto` | `margin-bottom: auto` |
| `margin-left` | Left margin | Value / `auto` | `margin-left: 20px` |
| `overflow` | Overflow handling (both axes) | `visible`, `hidden`, `scroll`, `auto` | `overflow: scroll` |
| `overflow-x` | Horizontal overflow (overrides X only) | same as `overflow` | `overflow-x: visible` |
| `overflow-y` | Vertical overflow (overrides Y only) | same as `overflow` | `overflow-y: auto` |
| `scrollbar-gutter` | Always reserve the scrollbar lane (constant viewport) | `stable` | `scrollbar-gutter: stable` |

`margin` supports `auto` for horizontal centering (set both left and right to `auto`).

`width`/`height`, `min-*`, and `max-*` always describe the border box, including padding and border.

`overflow: scroll` or `auto` shows scrollbars when content exceeds the container.

`overflow-x`/`overflow-y` override `overflow` per axis: on an axis set to `visible` the container neither clips nor shows a scrollbar there, and the overflow is handed to the nearest scrolling ancestor (so "scroll vertically only, share the parent's horizontal bar" = `overflow-y: auto; overflow-x: visible`). Box-size decisions (flex cross-axis stretch) still read the `overflow` shorthand.

`scrollbar-gutter: stable` makes a scrolling axis **always reserve** one scrollbar width: the viewport permanently gives up that lane, regardless of whether the content overflows or a bar is shown. The content area is therefore identical with and without a scrollbar, so the subtree never reflows (use it when a header above a list must stay aligned with it). The default (unset) reserves space only when a bar is actually shown.

- Applies only to axes whose `overflow` is `auto` / `scroll`; `visible` (no clipping) and `hidden` (no bar) axes are not affected.
- **Mind the default**: `overflow` defaults to `auto`, so a container that never sets `overflow` has both axes scrollable — `stable` then subtracts from both width and height (one bar width each). To reserve a vertical lane only, write `overflow-x: hidden` explicitly.
- X and Y are independent: with both axes scrollable, both reserve a lane.
- The lane sits **inside** the viewport: the declared `width`/`height` is unchanged, the usable content area loses one bar width.
- Lane width comes from this node's `scrollbar-width`; when unset, the global default thickness (8dp) is used.
- Write it in the `@style` block (same as `scrollbar-width`); in `@layout` it warns and has no effect.
- Difference from standard CSS: **only `stable` is recognized**. `auto` (or any other string) means off; `stable both-edges` is unsupported and — since the whole string is not `stable` — treated as off (without a warning).

Size constraints accept `auto`, absolute lengths, and percentages. Unset minimums are zero and unset maximums are unbounded. There is no implicit content-based minimum size (so, unlike CSS, flex items do not need `min-width: 0`).

### 6.5 Visual Effects

| Property | Description | Example |
|----------|-------------|---------|
| `opacity` | Opacity (0.0 = fully transparent, 1.0 = opaque) | `opacity: 0.8` |
| `visible` | Visibility | `visible: false` |
| `box-shadow` | Outer shadow (CSS syntax, multiple shadows allowed) | `box-shadow: "0 2px 8px #00000040"` |
| `background-image` | Background image path | `background-image: "images/bg.png"` |

**`box-shadow` syntax**: `[inset] <x-offset> <y-offset> [blur] [spread] <color>` (lengths in px or plain numbers,
the color may come first). Separate multiple shadows with commas — **the first one paints on top**:

```
box-shadow: "0 2px 8px #00000040";                       /* 2px down, 8px blur, 25% black */
box-shadow: "0 1px 2px #0000001f, 0 4px 12px #00000026"; /* two layers: tight + wide */
box-shadow: "rgba(0, 0, 0, 0.25) 0 0 12px 2px";          /* color first + 2px spread */
```

Colors accept `#RGB` / `#RRGGBB` / `#RRGGBBAA` / `rgb()` / `rgba()` / `transparent`; `none` (or omitting the
property) means no shadow. Semantics match CSS: the shadow is painted **outside the border box only**, the blur
fades outward and reaches `blur` pixels from the edge, and it follows `border-radius` (the radius grows with the
spread). **`inset` (inner shadows) is not supported yet** — it is ignored with a warning in the log. The shadow
sits below the element's own border and content (and follows normal sibling stacking), so use `z-index` to cover
other content.

**Render quality (host-side C++ config, process-wide)**: `loppe::setBoxShadowQuality()`, levels
`Gaussian` (default; analytic Gaussian on GL: smooth gradient, no banding) / `High` / `Medium` / `Low`
(concentric-ring approximation with 16/8/4 rings, for GPU-less embedded targets; lower = cheaper) /
`None` (skip). The `.lui` syntax is quality-agnostic — the same style runs on all three backends.

**Image paths (`background-image` and `image`'s `src`)** are local file paths with two bases:

| Form | Relative to |
|------|-------------|
| `images/bg.png` | **the exe's directory** (then `<exe dir>/resources/images/bg.png` as a fallback) |
| `~/images/bg.png` | **the directory of the .lui file itself** (use this for assets shipped next to the .lui) |

### 6.6 Overlay & Anchoring

| Property | Description | Accepted Values | Example |
|----------|-------------|-----------------|---------|
| `overlay` | Render in overlay layer (out of normal flow) | `true`, `false` | `overlay: true` |
| `anchor` | Overlay anchor position | `none`, `topleft`, `center`, `topright`, `bottomleft`, `bottomright` | `anchor: center` |
| `z-index` | Stacking order among siblings | integer, default `0` | `z-index: 100` |

`overlay: true` takes the node out of normal layout flow, displaying it above other content. `anchor` picks the
place it sticks to: `center` and the three corners (`topright`/`bottomleft`/`bottomright`) are positioned against
the **parent's content box**, and `margin` is the inset from that edge. `topleft` is the exception — it means
"window origin + `offset`" in absolute coordinates (popups rely on writing `offset` in screen coordinates).
Spell the values **without a hyphen** (`top-left` parses as `none` and silently does not anchor).

`z-index` orders **siblings under the same parent**: a larger value paints on top; equal values keep declaration
order (later node on top); negative values go below the default `0` siblings. It also drives hit testing (the
topmost sibling gets the click first), so the layer that blocks input is always the layer you can see.
`z-index` is applied when the tree is built; changing it at runtime through a class toggle does not reorder
siblings (rebuild the tree instead).

---

## 7. Layout Properties

Layout properties are used in the `@layout` block and can also be inlined in node attribute parentheses.

> ⚠️ **Get the block right**: layout properties are compiled only inside `@layout`; writing them in a
> `@style` block is **silently ignored** (no error, no warning — the UI just looks wrong). Conversely,
> `width` / `height` / `max-height` / `min-*` are **size** properties compiled only in `@style`
> (or inline); writing them in `@layout` is silently ignored too.
> Rule of thumb: `@layout` holds "how things are arranged" (the table in §7), everything else — including
> sizes — goes in `@style`.

| Property | Description | Accepted Values | Example |
|----------|-------------|-----------------|---------|
| `display` | Layout mode | `block`, `inline`, `flex`, `grid`, `none` | `display: flex` |
| `position` | Positioning | `static`, `relative`, `absolute`, `fixed` | `position: absolute` |

### 7.1 Flex Layout Properties

When `display` is set to `flex`, the following properties control flex layout behavior:

| Property | Description | Accepted Values | Default |
|----------|-------------|-----------------|---------|
| `flex-direction` | Main axis direction | `row`, `column`, `row-reverse`, `column-reverse` | `row` |
| `align-items` | Cross-axis alignment | `stretch`, `flex-start`, `flex-end`, `center`, `baseline` | `stretch` |
| `align-self` | Per-child cross-axis alignment (overrides parent `align-items`) | `auto`, `stretch`, `flex-start`, `flex-end`, `center`, `baseline` | `auto` |
| `justify-content` | Main-axis alignment | `flex-start`, `flex-end`, `center`, `space-between`, `space-around`, `space-evenly` | `flex-start` |
| `flex-wrap` | Wrapping | `nowrap`, `wrap`, `wrap-reverse` | `nowrap` |
| `flex-grow` | Flex grow ratio | Number | `0` |
| `flex-shrink` | Flex shrink ratio | Number | `0` |
| `gap` | Child spacing | Value + unit | `0` |

### 7.2 Grid Layout Properties

When `display` is set to `grid`:

| Property | Description | Example |
|----------|-------------|---------|
| `grid-template-columns` | Column template | `grid-template-columns: "1fr 2fr 1fr"` |
| `grid-template-rows` | Row template | `grid-template-rows: "auto 1fr auto"` |
| `align-items` | Block-axis (vertical) alignment | `align-items: center` |
| `align-self` | Per-child block-axis alignment (overrides parent `align-items`) | `align-self: flex-end` |

---

## 8. Layout Model

See [LAYOUT_DESIGN_EN.md](LAYOUT_DESIGN_EN.md) for the complete deterministic sizing, constraint, flex, and overflow rules.

### 8.1 Block Layout (Default)

`display: block` is the default layout mode. Characteristics:

- Child elements **stack vertically**, one after another
- Child width defaults to **fill the parent container's** content width
- Height is determined by child content

Suitable for page structure, card lists, and other top-to-bottom arrangements.

### 8.2 Flex Layout

`display: flex` enables flex layout. **Note**: LOPPE's Flex implementation is a curated subset of features, including the most commonly used Flex capabilities but not the full CSS Flexbox standard.

#### Supported Features

**Direction Control:**
- Four main axis directions: `row` (left to right), `column` (top to bottom), `row-reverse`, `column-reverse`
- Reverse layouts render correctly

**Wrapping:**
- `nowrap` (default): Single line, children may overflow
- `wrap`: Auto-wrap to next line/column when exceeding container size

**Main-Axis Alignment (justify-content) — all 6 modes:**
- `flex-start` — Align to start
- `flex-end` — Align to end
- `center` — Center alignment
- `space-between` — Space between, evenly distributed
- `space-around` — Equal spacing around each child
- `space-evenly` — All spacing (including ends) completely equal

**Cross-Axis Alignment (align-items) — 4 modes:**
- `stretch` (default) — Auto cross sizes may stretch; they shrink to the container cross size only when overflow is not `visible`
- `flex-start` — Align to start
- `flex-end` — Align to end
- `center` — Center alignment

**Per-Child Override (align-self):**
- Written on a child; overrides the parent container's `align-items`; `auto` (default) means follow the parent
- Values: `stretch` / `flex-start` / `flex-end` / `center` (`baseline` has no effect)

**Flex Grow & Shrink:**
- `flex-grow`: Flex grow ratio. When container has extra space, distributes proportionally. Default 0 (no grow)
- `flex-shrink`: Flex shrink ratio. When space is insufficient, the deficit is distributed directly by shrink value. Default 0 (no shrink)
- Only items with `flex-shrink > 0` participate; an item freezes at its explicit minimum and remaining deficit is redistributed
- After grow/shrink, affected children and descendants are automatically re-measured

**Spacing:**
- `gap`: Uniform spacing between children

#### Unsupported Features

The following Flex features are **not yet implemented**:

| Unsupported Feature | Notes |
|--------------------|-------|
| `align-items: baseline` | Cannot align by text baseline (enum exists but has no effect) |
| `flex-basis` | No explicit initial main-axis size; child size determined by `width`/`height` only |
| `flex` shorthand | Cannot write `flex: 1`; use separate `flex-grow` and `flex-shrink` |
| `order` | Cannot change visual order of children; order follows source position |
| `align-content` | Multi-line cross-axis distribution of rows is uncontrollable; simple stacking only |

### 8.3 Grid Layout

`display: grid` enables two-dimensional grid layout. **Note**: LOPPE's Grid is a minimal implementation — rows and columns are defined together and children are auto-placed into cells in source order; it does not implement the full CSS Grid standard.

#### Track Templates

`grid-template-columns` defines column tracks; `grid-template-rows` defines row tracks. Multiple values are space-separated:

| Track value | Meaning |
|-------------|---------|
| `100px` | Fixed width |
| `50%` | Percentage of the container content width |
| `1fr` | Flexible track: distributes remaining space proportionally (e.g. `1fr 2fr` means the second column is twice as wide as the first) |
| `auto` | Sized by the widest item on that track |

When `grid-template-rows` is omitted, row heights are content-driven.

#### Behavior

- **Auto placement**: children fill cells left-to-right, top-to-bottom, wrapping to a new row when a column fills up
- **Implicit rows**: extra children beyond the declared rows create new rows automatically, sized by content
- **Stretch**: items with `width`/`height` set to auto stretch to fill their cell (minus their own margin/border/padding)
- **align-items / align-self**: block-axis (vertical) alignment. Default `stretch` (fills the row height); with `center` / `flex-end` / `flex-start` items keep their content height and align within the row; a child may write `align-self` to override individually
- **gap**: uniform row/column spacing; participates in remaining-space calculation

#### Example

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

#### Limitations

| Unsupported Feature | Notes |
|--------------------|-------|
| `grid-column` / `grid-row` | Cannot place or span items manually; auto placement only |
| `grid-template-areas` | Named areas not supported |
| `minmax()` | Track min/max sizes not supported |
| `grid-auto-rows` | Implicit row height is always content-driven |
| `justify-items` / `justify-content` / `align-content` | Inline-axis alignment within cells and whole-grid alignment are not configurable (use `align-items` / `align-self` for the block axis) |

### 8.4 Inline Layout

`display: inline` enables inline layout. Characteristics:

- Child elements **arrange horizontally**, like text flow
- **Auto-wrap** when exceeding container width
- Each child maintains its intrinsic width

Suitable for tag groups, text + icon mixed arrangements, etc.

### 8.5 Box Model

Each widget is a rectangular box with four layers from outside to inside:

```
┌─────────────────────────────┐
│         margin              │  ← Margin (spacing from other widgets)
│  ┌───────────────────────┐  │
│  │       border          │  │  ← Border
│  │  ┌─────────────────┐  │  │
│  │  │    padding      │  │  │  ← Padding (space between border and content)
│  │  │  ┌───────────┐  │  │  │
│  │  │  │  content  │  │  │  │  ← Content area (text, images, etc.)
│  │  │  └───────────┘  │  │  │
│  │  └─────────────────┘  │  │
│  └───────────────────────┘  │
└─────────────────────────────┘
```

- `width`/`height` always control the border box, including padding and border

---

## 9. Complete Examples

### 9.1 Simple Login Panel

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
        text(class="title", value="Account Login"),
        input:username(class="input-field", placeholder="Enter username"),
        input:password(class="input-field", placeholder="Enter password", type="password"),
        button:loginBtn(class="login-btn") { text(value="Login") }
    }
}
```

### 9.2 Horizontal Product List

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
        text(class="product-name", value="Product Name"),
        text(class="product-price", value="$99")
    },
    panel(class="product-card") {
        image(class="product-image", src="p2.png"),
        text(class="product-name", value="Product Name"),
        text(class="product-price", value="$199")
    },
    panel(class="product-card") {
        image(class="product-image", src="p3.png"),
        text(class="product-name", value="Product Name"),
        text(class="product-price", value="$299")
    }
}
```

### 9.3 Page Layout with Scrollable Content

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
        text(class="navbar-title", value="App Title")
    },
    panel(class="content") {
        /* Scrollable content area */
        text(value="Scrollable content goes here...")
    },
    panel(class="tabbar") {
        panel(class="tab-item active") { text(value="Home") },
        panel(class="tab-item") { text(value="Discover") },
        panel(class="tab-item") { text(value="Messages") },
        panel(class="tab-item") { text(value="Profile") }
    }
}
```

---

## 10. Common Widget Types

| Type | Description | Common Attributes |
|------|-------------|-------------------|
| `panel` | Container / panel | Generic style and layout attributes |
| `text` | Text label | `value` (text content) |
| `button` | Button | Set label via child `text` node |
| `input` | Input field | `placeholder` (placeholder text), `type` (input type) |
| `combobox` | Dropdown list | `options` (comma-separated items), `arrow` (icon name for the expander arrow; defaults to `chevron-down`, empty string = no arrow) |
| `progress` | Progress bar | `value` (declared progress 0–1); track = own background (`background-color` / `border-radius`) |
| `icon` | Icon | `name` (registered icon name, see `SvgRegistry`) |
| `image` | Image | `src` (image path; `~/` = the .lui's own directory) |
| `radio` | Radio button | see `src/components/radio*` |
| `scrollbar` | Scrollbar | Managed by the engine; you normally do not create it manually |

> The `combobox` arrow is a **built-in engine icon** (defaults to `chevron-down`; `chevron-up` / `chevron-right` /
> `chevron-left` also work in `arrow`), so no host registration is needed. `arrow=""` hides it, and `setArrowIcon`
> changes it at runtime. Only restyle it, or point `arrow` at your own icon name, by registering via
> `svg::RegisterIcon` (same name overrides; unregistered names draw nothing).

**Runtime updates (widget APIs; UI thread — from a worker thread post via `UiDispatcher`)**

| Widget | API | Notes |
|--------|-----|-------|
| `text` | `setText` | Change the text (writing back to the AST is the host's decision) |
| `combobox` | `setOptions` (**replaces**; safe to call repeatedly to refresh) / `setSelectedIndex` / `getSelectedIndex` / `setPlaceholder` / `setOnWillOpen` (refresh items before opening) / `setArrowIcon` (change the expander arrow icon) | Selection changes dispatch a `Change` event through `UIEventSystem` → listen with `scene->onChange("#id", cb)` (`ev.text` = selected item text) |
| `progress` | `setProgress(0..1)` / `setBarColor` | The fill is drawn by the widget; **do not hand-draw a progress bar inside `Scene::render()`**. Declaring `value` in the description file sets the progress; otherwise the host sets it |
| `input` | `setValue` / `focus()` | After focus the caret blink is driven by the widget itself (see below) |

> Caret-blink style widgets (time-driven, appearance changes on only a few frames): call
> `requestAnimationTick()` from `update()` to keep ticking and **only call `invalidate()` when
> something really changed** — `invalidate()` requests a repaint, so calling it every frame means a
> full-scene repaint every frame (wasted CPU/GPU; see `knowledge/repo-memory/caret-blink-repaint.md`).
