# LOPPE DSL Syntax Reference (AI Edition)

> Compact version for AI context injection. Full property set, syntax rules, and key differences from standard CSS.

> 📖 [中文版](LOPPE_DSL_AI_REFERENCE.md) | English

> Building / running: always `loppe-cli build . Debug` (after editing `build.lpj`, run `loppe-cli config .` first).
> Projects have no `CMakeLists.txt` — do not use cmake / make / msbuild, and do not hand-write clang command lines.
> Arguments and output locations: see `../LOPPE_CLI_GUIDE_EN.md`.
>
> **A project is not limited to one UI file**: it may hold several `.lui` files (a shell leaves
> `panel(id="…")` as a mount point, one file per page, mounted at runtime with
> `ComponentInstantiator::SpawnElementViewUnderId`). Before editing, count the `.lui` files in the
> UI directory and look for such mounts / repeated `loadUi` calls in the C++; do not assume
> `main.lui` is the only one. Mounting, unmounting, shared styles, and the multi-file `EMBED_FILES`
> form on ESP32: see `../LOPPE_MULTI_UI_EN.md`.
>
> **A `.lui` has no template / placeholder syntax**: never write markers such as `@ROWS@` or `@ITEMS@`
> and substitute them in C++ — `@` is only valid in `@style` / `@layout` / `@version`, anything else is a
> parse error. For list rows, either build the row DSL in C++ and mount it into an empty container
> (`SpawnElementViewUnderId`), or use `ComponentHost::SetRows`, or self-draw. See
> `../LOPPE_MULTI_UI_EN.md` §2.6.

---

## 1. Quick Start

An LOPPE DSL file consists of three kinds of blocks:

```loppe
@version(1.0)                      // Optional: UI DSL version required by this file (major only; omitted = no check)
@style    { /* Style rules */ }
@layout   { /* Layout rules */ }
// Node tree (widget hierarchy)
panel(id="root") {
    text(value="Hello")
}
```

`@style` and `@layout` blocks must appear before the node tree. `@version(...)` (optional) declares the UI DSL
version and has exactly **one** spelling — `@version(1.1.0)` (parentheses around a bare version number, plain
numeric segments; parentheses
omitted or `@version("1.1.0")` is an error). Only a matching engine **major** version passes; a different major fails
parsing immediately (current engine version is `loppe::engineVersion()`, whose own string may carry a `-preview.1`
style suffix), and it may appear only once per file.

> **Touch soft keyboard**: the engine ships a built-in soft keyboard (`loppe::SoftKeyboard`). It pops up automatically
> when an `input` gains focus and hides when focus leaves; touches on it are not passed through and keys go straight to
> the input. One implementation for all platforms, zero host code. There is no DSL tag for it; see
> `../LOPPE_BUILTIN_CONTROLS_EN.md` §4.11.

---

## 2. Selector Quick Reference

| Syntax | Matches | Example |
|--------|---------|---------|
| `type` | Widget type name | `panel`, `text`, `button` |
| `.class` | class attribute value | `.header`, `.active` |
| `#id` | id attribute value | `#root` |
| `:pseudo` | UI state | `:hover`, `:active`, `:focus`, `:disabled` |
| Combined | Concatenated in order | `panel.login-panel:hover` |

**Limitation**: Descendant/child combinators are not supported (no `panel .title` or `panel > .title`). A selector matches only a single node.

---

## 3. Node Syntax

```
type                       // Simplest
type:id                    // With id
type(attr=val, ...)        // With attributes
type:id(attr=val, ...)     // Full form
```

**Attribute list syntax** (inside parentheses, comma-separated):
```
key=value                  // Single value
key="string"               // String
key=[item1, item2]         // Array
key.subkey=value           // ❌ Not supported (no nesting)
```

**Child node syntax** (inside curly braces, comma-separated):
```
type(attr) { child1, child2, child3(id="x") }
```

**Automatic value type inference:**

| Token Form | Inferred As |
|------------|-------------|
| `42` | int |
| `3.14`, `1e5` | float |
| `"hello"` | string |
| `true`, `false` | bool |
| `100px`, `50%`, `auto` | string (valued numbers with unit kept as string) |
| Other identifiers | string |

String escapes: `\n`, `\r`, `\t`, `\"`, `\\`. A `\t` is laid out on tab stops (one every `4 × font-size` from the
start of the display row; each wrapped row re-aligns to its own row start), fills the gap up to the next stop, and
draws nothing; use `\t` instead of runs of spaces for table-like alignment.

---

## 4. Unit System

| Unit | Meaning | Example |
|------|---------|---------|
| `px` | Pixels | `width: 100px` |
| `dp` | Density-independent pixels (equivalent to px) | `width: 50dp` |
| `sp` | Scale-independent pixels | `font-size: 14sp` |
| `%` | Percentage of parent | `width: 50%` |
| `auto` | Automatic calculation | `width: auto` |
| Bare number | Defaults to px | `width: 100` → 100px |

**Colors**: Only `#RRGGBB`, `#RRGGBBAA` or `#RGB` hex format is supported; `AA` is the alpha channel (`00` fully transparent, `ff` opaque). All backends (desktop GL / Android / embedded soft rasterizer) blend it as "color alpha × opacity" over what is beneath.

---

## 5. Style Properties — Complete Table

### 5.1 Color & Background

| Property | Type | Default | Inherits | Description |
|----------|------|---------|----------|-------------|
| `background-color` | Color | `#00000000` (transparent) | No | Background color |
| `color` | Color | `#000000` | **Yes** | Text color |
| `border-color` | Color | `#00000000` | No | Border color |
| `background-image` | string | `""` | No | Background image path (`~/` = the .lui's own directory) |

### 5.2 Borders

| Property | Type | Default | Inherits | Description |
|----------|------|---------|----------|-------------|
| `border` | shorthand | — | No | Shorthand: `"1px #ff0000"` (width color; keywords like `solid` are ignored) |
| `border-width` | Edges | `0` | No | Uniform width for all four sides |
| `border-width-top` | float | `0` | No | Top border |
| `border-width-right` | float | `0` | No | Right border |
| `border-width-bottom` | float | `0` | No | Bottom border |
| `border-width-left` | float | `0` | No | Left border |
| `border-radius` | px | `0` | No | Corner radius |

### 5.3 Typography

| Property | Type | Default | Inherits | Description |
|----------|------|---------|----------|-------------|
| `font-size` | px | `14` | **Yes** | Font size |
| `font-family` | string | `""` | **Yes** | Font name |
| `font-weight` | keyword | `normal` | **Yes** | `normal` / `bold` |
| `line-height` | px | `0` | **Yes** | Line height; 0 = auto |
| `text-align` | keyword | `left` | **Yes** | `left` / `center` / `right` / `justify` |

### 5.4 Box Model

| Property | Type | Default | Inherits | Description |
|----------|------|---------|----------|-------------|
| `width` | Unit | `auto` | No | px/dp/sp/%/auto |
| `height` | Unit | `auto` | No | px/dp/sp/%/auto |
| `padding` | Edges | `0` | No | All four sides padding |
| `padding-top/right/bottom/left` | px | `0` | No | Per-side padding |
| `margin` | Edges | `0` | No | All four sides margin; supports `auto` |
| `margin-top/right/bottom/left` | px | `0` | No | Per-side margin; supports `auto` |
| `overflow` | keyword | `auto` | No | `visible` / `hidden` / `scroll` / `auto` (shorthand: sets both axes) |
| `overflow-x` | keyword | same as `overflow` | No | Overrides the horizontal axis only; `visible` = no clipping, no horizontal bar, horizontal overflow is handed to the nearest scrolling ancestor |
| `overflow-y` | keyword | same as `overflow` | No | Overrides the vertical axis only (same semantics) |
| `scrollbar-gutter` | keyword | off | No | **Only `stable`**: the scrolling axis always reserves one lane (viewport permanently loses one bar width, identical content area with or without a bar). Applies to `auto`/`scroll` axes only, both axes independent, lane is inside the viewport; `overflow` defaults to `auto`, so without an explicit `overflow` both axes reserve (width and height each lose one bar width) — write `overflow-x: hidden` to reserve vertically only; `auto` / `stable both-edges` / any typo = off. Written in `@style` |

### 5.5 Visual Effects

| Property | Type | Default | Inherits | Description |
|----------|------|---------|----------|-------------|
| `opacity` | float | `1.0` | No | Opacity (0.0 ~ 1.0) |
| `visible` | bool | `true` | **Yes** | Visibility |
| `box-shadow` | string | `""` | No | Outer shadow: `[inset] <x> <y> [blur] [spread] <color>`, comma-separated layers (first paints on top). Painted outside the border box only, follows `border-radius`; `inset` is not supported yet (ignored with a warning) |
| `overlay` | bool | `false` | No | Render in overlay layer (out of flow) |
| `anchor` | keyword | `none` | No | `none` / `topleft` / `center` / `topright` / `bottomleft` / `bottomright` (no hyphen; `topleft` = absolute coords + offset, the rest = parent content box + margin) |
| `z-index` | int | `0` | No | Sibling stacking order: larger paints on top; equal keeps declaration order; negative sinks below `0`. Also drives hit-test order (applied when the tree is built; a runtime class toggle does not reorder) |

---

## 6. Layout Properties — Complete Table

These can be used in `@layout` blocks or inlined in node attributes.
**Wrong block = silently ignored**: layout properties written in `@style`, and
`width`/`height`/`min-*`/`max-*` written in `@layout`, are not compiled and raise no error (see §7.2 and the user guide §7).

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `display` | keyword | `block` | `block` / `inline` / `flex` / `grid` / `none` |
| `position` | keyword | `static` | `static` / `relative` / `absolute` / `fixed` |
| `flex-direction` | keyword | `row` | `row` / `column` / `row-reverse` / `column-reverse` |
| `align-items` | keyword | `stretch` | `stretch` / `flex-start` / `flex-end` / `center` / `baseline` |
| `align-self` | keyword | `auto` | Per-child override of parent `align-items`: `auto` / `stretch` / `flex-start` / `flex-end` / `center` / `baseline` |
| `justify-content` | keyword | `flex-start` | `flex-start` / `flex-end` / `center` / `space-between` / `space-around` / `space-evenly` |
| `flex-wrap` | keyword | `nowrap` | `nowrap` / `wrap` / `wrap-reverse` |
| `flex-grow` | float | `0` | Flex grow ratio |
| `flex-shrink` | float | `0` | Flex shrink ratio; participates only when explicitly enabled |
| `gap` | px | `0` | Child spacing |
| `grid-template-columns` | string | `""` | Grid column template |
| `grid-template-rows` | string | `""` | Grid row template |

---

## 7. Flex Implementation Details & Limitations

### 7.1 Implemented Features

| Feature | Status | Notes |
|---------|--------|-------|
| `flex-direction` | ✅ | row / column / row-reverse / column-reverse; reverse works correctly |
| `flex-wrap` | ✅ wrap/nowrap | Wrap works; wrap-reverse enum exists but row stacking order may be non-standard |
| `justify-content` | ✅ All 6 values | flex-start / flex-end / center / space-between / space-around / space-evenly |
| `flex-grow` | ✅ | Proportional distribution of remaining space; triggers child remeasure after allocation |
| `flex-shrink` | ✅ | Direct shrink-value allocation; freezes at min-size and redistributes the remainder |
| `align-items: stretch` | ✅ | Auto cross sizes may grow; they shrink to the container only when `overflow != visible` |
| `align-items: center` | ✅ | Cross-axis centering |
| `align-items: flex-end` | ✅ | Cross-axis end alignment |
| `align-items: flex-start` | ✅ | Cross-axis start (default fallthrough) |
| `align-self` | ✅ | Per-child override of parent `align-items` (auto = follow parent) |
| `gap` | ✅ | Child spacing; participates in wrap and space allocation |
| Shrink floor | ✅ | Uses only explicit min-size; the default is zero with no content-based implicit floor |
| Nested remeasure | ✅ | Auto-remeasure descendants after grow/shrink/stretch |

### 7.2 ⚠️ Not Implemented / Non-Standard Behavior

| Missing Feature | Impact | Workaround |
|----------------|--------|------------|
| **`align-items: baseline`** | Enum exists but no code path; falls back to flex-start | Fixed height + center approximation |
| **`flex-basis`** | No explicit initial main-axis size; `flex-grow` is based on the **content size**, so "equal-width columns" cannot be achieved with `flex-grow: 1` alone — as soon as one column's content changes, that column grows wider and the others are squeezed | Give a definite size: put `width` (e.g. `width: 25%`) in **`@style`** together with `flex-grow`, or use a fixed px width |
| **`flex` shorthand** | Cannot use `flex: 1` | Write `flex-grow` + `flex-shrink` separately |
| **`order`** | Cannot change visual order | Reorder nodes in source |
| **`align-content`** | Multi-line cross-axis distribution uncontrollable | Rows simply stack |
| **`row-gap / column-gap`** | Only single `gap` | — |
| **Percentage gap** | gap is absolute length only | — |
| **`wrap-reverse`** | Enum exists but cross-axis may be non-standard | Prefer wrap |
| **Flex child `margin:auto`** | Does not absorb remaining space | Normal box-model behavior only |

### 7.3 Grid Implementation Details & Limitations

#### Implemented Features

| Feature | Status | Notes |
|---------|--------|-------|
| `grid-template-columns` | ✅ | px / % / fr / auto tracks, space-separated |
| `grid-template-rows` | ✅ | Same as above; row height is content-driven when omitted |
| `gap` | ✅ | Uniform row/column spacing; participates in remaining-space calculation |
| Auto placement | ✅ | Row-major cell filling |
| Implicit rows | ✅ | Auto rows are appended when items exceed the declared rows |
| Stretch | ✅ | Auto-sized items fill their cell by default (minus own margin/border/padding) |
| `align-items` / `align-self` | ✅ | Block-axis (vertical) alignment: stretch (default) / center / flex-end / flex-start; per-child `align-self` overrides |

#### fr Allocation Rules

- Fixed tracks (px / % / auto) claim space first
- Remaining space = container content size − fixed tracks total − gap total, distributed to `fr` tracks proportionally to their `fr` values
- When the container size is unknown (auto), `fr` tracks resolve to 0 and the container width is decided by px / auto tracks

#### ⚠️ Not Implemented

| Missing Feature | Impact |
|----------------|--------|
| `grid-column` / `grid-row` | Cannot place or span items manually; auto placement only |
| `minmax()` | Track min/max sizes not supported |
| `grid-auto-rows` | Implicit row height is always content-driven |
| `grid-template-areas` | Named areas not supported |
| `justify-items` / `justify-content` / `align-content` | Inline-axis alignment within cells and whole-grid alignment are not configurable (use `align-items` / `align-self` for the block axis) |

---

## 8. Layout Algorithm Behavior

Deterministic rules: `width`/`height` are border-box preferred sizes; min-size defaults to zero and max-size is unbounded; grow and shrink are independent; only items with explicit `flex-shrink > 0` may shrink; deficit is distributed directly by shrink value, freezing items at min-size and redistributing the remainder; unresolved deficit becomes overflow. In a single flex line, an auto cross size may always grow through stretch; it shrinks to the container cross size only when the item uses auto, scroll, or hidden overflow. With visible overflow, content size is preserved and overflow is propagated to the parent.

| Layout Mode | display value | Behavior |
|------------|---------------|----------|
| **Block** | `block` (default) | Vertically stacked. Children fill parent width by default; height is content-driven |
| **Flex** | `flex` | Flexbox layout (non-standard CSS subset). See §7. Does not support baseline/flex-basis/order/align-content (align-self is supported) |
| **Inline** | `inline` | Horizontal flow layout. Children wrap automatically, similar to text flow |
| **Grid** | `grid` | Two-dimensional grid layout. Supports px/%/fr/auto tracks, gap, auto-placement, implicit rows, stretch, and align-items/align-self block-axis alignment (see §7.3) |
| **Hidden** | `none` | Excluded from layout and rendering |

Box model (outside → inside): **margin → border → padding → content**

`width`/`height`, `min-*`, and `max-*` always describe the border box, including padding and border.

Containers with `overflow: scroll` or `auto` provide scrollbars and `scrollWidth`/`scrollHeight` when content overflows.

---

## 9. ⚠️ Key Differences from Standard CSS

| # | Scenario | ❌ Wrong (CSS habit) | ✅ Correct (LOPPE DSL) |
|---|----------|---------------------|---------------------|
| 1 | Descendant selector | `.panel .title` | Assign a class directly to the child node; combinators not supported |
| 2 | Child selector | `panel > text` | Same as above |
| 3 | Border shorthand | `border: 1px solid #ccc` | `border: 1px #cccccc` (solid keyword is ignored) |
| 4 | em/rem units | `font-size: 1.2em` | `font-size: 16px` or `font-size: 16sp` |
| 5 | vh/vw units | `height: 100vh` | `height: 100%` (parent must have a definite height) |
| 6 | Flex shorthand | `flex: 1` | `flex-grow: 1; flex-shrink: 1` |
| 7 | flex-basis | `flex-basis: 200px` | ❌ Not supported; use `width`/`height` instead (in `@style`); equal-width columns (e.g. a tab bar) need an explicit `width` — `flex-grow` alone grows with content |
| 8 | align-self | `align-self: center` | ✅ Supported; overrides parent `align-items` on flex/grid children (auto = follow parent) |
| 9 | order | `order: 1` | ❌ Not supported; reorder nodes in source |
| 10 | align-content | `align-content: space-between` | ❌ Not supported; multi-line simply stacks |
| 11 | align-items: baseline | `align-items: baseline` | ❌ No effect, falls back to flex-start |
| 12 | position:absolute coordinates | `position: absolute; top: 0; left: 0` | `overlay: true; anchor: topleft` |
| 13 | visibility/display:none | `visibility: hidden` | `visible: false` |
| 14 | display:none | `display: none` | `visible: false` or `display: none` |
| 15 | rgba() colors | `rgba(0,0,0,0.5)` | `background-color: #00000080` (last two digits = alpha) |
| 16 | CSS specificity | Cascade by selector weight | Rules override in declaration order (last wins) |
| 17 | min/max-width/height | `min-width: 100px` | ✅ Lightweight support for `auto`, absolute lengths, and percentages |
| 18 | Pseudo-elements | `::before`, `::after` | ❌ Not supported |
| 19 | @media queries | `@media (max-width: 768px)` | ❌ Not supported |
| 20 | transform/transition | `transform: rotate(45deg)` | ❌ Not supported |
| 21 | z-index | `z-index: 10` | ✅ Works among siblings (ordering is fixed at tree build; a runtime class toggle does not reorder) |
| 22 | Color functions | `rgb()`, `hsl()` | Only `#RRGGBB` / `#RRGGBBAA` / `#RGB` (exception: `box-shadow` strings also accept `rgb()` / `rgba()`) |
| 23 | border-style | `border-style: dashed` | ❌ Not supported (border is width + color only) |
| 24 | background shorthand | `background: url(...) center` | `background-image: "path"` (path only) |
| 25 | Flex item minimum size | `min-width: 0` (needed in CSS so a flex item can shrink below its content) | ❌ Not needed: there is no implicit content minimum; `min-width` already defaults to `0` |
| 26 | scrollbar-gutter value | `scrollbar-gutter: stable both-edges` | Only `stable` is recognized; `both-edges` is unsupported (the whole string means off, and the opposite side is not reserved); the lane always sits inside the scrolling axis, in `@style` |

---

## 10. Complete Examples

### 10.1 Login Panel

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
        text(class="title", value="User Login"),
        input(class="field", placeholder="Username"),
        input(class="field", placeholder="Password", type="password"),
        button(class="btn") { text(value="Login") }
    }
}
```

### 10.2 Flex Horizontal List

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

### 10.3 Scrollable Container

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

## 11. Host C++ Timing (AI Must-Read When Generating C++)

The easiest thing to get wrong when generating "load the interface + fill controls with data" code is **timing** — a mistake never raises an error, it just shows up as "the interface looks fine but the data is empty".

| Phase | You can | You cannot |
|-------|---------|------------|
| `Scene::setup()` | read the `.lui` → `notifyASTUpdated(ast)`; `setDocumentRoot`; bind events by selector `onClick("#id")` / `onChange("#id")` / `onInput("#id")` | **look a control up and mutate it**: controls do not exist yet, `findControlById()` silently returns `nullptr` |
| `Scene::onControlsReady()` | every control-mutating initialization: `setValue` / `setOptions` / `setSelectedIndex` / `setProgress` | run only once (it may fire several times per frame and a re-parse replaces control instances → must be idempotent) |

- `onControlsReady()` fires after the first frame's AST sync and before rendering; **a tree rebuild replaces control instances**, so re-fetch them on every call.
- A `combobox` must be fed by the host with `setOptions({...})`: `options="a,b"` in the `.lui` only covers static lists and the comma is the separator. **With zero options the popup height is 0**, which users read as "clicking does nothing" — the control is fine.
- The `combobox` arrow is a **built-in engine icon** (defaults to `chevron-down`; only `arrow=""` hides it), so the
  host registers nothing. Any other icon name must be registered with `svg::RegisterIcon`, otherwise that spot stays
  blank.
- The popup lives on the **overlay layer** (drawn above the main tree): a control cannot push it down, so do not try to fix stacking with `z-index`.

---

## 12. DSL vs HTML+CSS — Key Design Differences (AI Must-Read)

The following differences are by design in the DSL engine, unlike standard CSS behavior. Keep them in mind when generating DSL.

### 11.1 ⭐ Default `overflow` is `auto` (CSS default is `visible`)

In standard CSS, block elements default to `overflow: visible` (content overflows without clipping).
**LOPPE DSL defaults to `overflow: auto`** — if content exceeds the container's width/height, a scrollbar appears automatically.

```loppe
/* No overflow set → defaults to auto */
.panel { height: 100px; }  
/* If children exceed 100px → vertical scrollbar appears */
```

> To clip without scrollbar: explicitly write `overflow: hidden`
> To allow free overflow: explicitly write `overflow: visible`

⚠️ As a result, **every container is a scroll container by default** (including `display: flex` list containers): once content grows, it can sprout its own scrollbar — even when the parent still has room. If an outer element should be the only scroller (e.g. "the column `.w-main-col` is the single scroll container"), write `overflow: visible` **explicitly** on the inner containers (it preserves the content size and hands the overflow to the parent) instead of relying on the default.

### 11.2 ⭐ LOPPE always uses border-box sizing

`width`, `height`, `min-*`, and `max-*` include padding and border. LOPPE does not support switching to content-box sizing.

```loppe
/* The border-box height equals the parent height; padding is included. */
.card { height: 100%; padding: 24px; }
```

`height: 100%` and padding can therefore be combined directly without a `box-sizing` property.

### 11.3 Fixed-size containers must account for content

Because `overflow` defaults to `auto`, if you set a fixed `height` or `width` on a container whose children exceed that size, a **scrollbar will appear automatically**. This is by design, not a bug.

```loppe
/* If content is ~35px tall, fixing height to 30px → scrollbar appears */
.progress-area { height: 30px; }

/* Remove fixed height or use auto to let content determine the size */
.progress-area { }
```

### 11.4 `background-image` only supports local file paths

HTTP URLs are not supported. Images are loaded from the local filesystem.

```loppe
/* ❌ Not supported */
background-image: "https://example.com/image.jpg";

/* ✅ Local path only */
background-image: "images/cover.png";
```

Two path bases: `images/cover.png` (no prefix) = **the exe's directory**; `~/images/cover.png` = **the directory of the .lui file itself** (`image`'s `src` follows the same rule).

### 11.5 CJK fonts do not include emoji glyphs

The built-in font `AlibabaPuHuiTi` does not contain emoji. Using emoji characters (🔀 ⏮ ⏸ etc.) will render as tofu boxes `□`. Use English text instead.

```loppe
/* ❌ Not rendered */    text(value="🔀")
/* ✅ Works */            text(value="RDM")
```

### 11.6 Nested containers: calculate content-box height layer by layer

```
Root (800×600)
  └── Container (100% × 100%)        = 800×600
       └── Card (height:100%) = 360×600
            └── content-box = 600 - 32padding = 568px
                 └── children total ~572px → overflow 4px → scrollbar appears
```

When generating multi-layer nested DSL, calculate layer by layer whether the content-box can accommodate the sum of its children.

---

## Appendix: Inherited Properties

The following properties are automatically inherited from the parent node (if not explicitly set on the child, the parent's value is used):

`color`, `font-size`, `font-family`, `font-weight`, `line-height`, `text-align`, `visible`
