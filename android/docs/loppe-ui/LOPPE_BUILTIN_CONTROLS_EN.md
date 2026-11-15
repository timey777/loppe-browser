# LOPPE Built-in Controls Reference

> 📖 [中文版](LOPPE_BUILTIN_CONTROLS.md) | English

---

## 1. What This Document Covers

Every node in the interface tree is a control, and the tag name decides the control type. This document lists the built-in control tags one by one, giving each control's attributes, default appearance, available pseudo-states, dispatched events, C++ type, runtime interfaces, and a minimal example.

**Audience**: people writing `.lui` interfaces, and people who need to drive controls from C++. Prerequisites: a project already running per *From C++ to UI*, or sections 2–4 of the *DSL User Guide*.

**Contents**

| What you are looking for | Section |
|---|---|
| Rules shared by every control (pseudo-states, disabling, sizing) | §2 |
| A single table of all controls | §3 |
| One control's attributes and example | §4 (one subsection per control) |
| Which control dispatches which event | §5 |
| Limitations and common problems | §6 |

---

## 2. General Rules

### Tags and node attributes

- The tag name is the control type name (lowercase), one type per node: `panel`, `text`, `button`, `input`, `combobox`, `radio`, `progress`, `icon`, `image`. `scrollbar` is created by the engine on demand and is normally not written by hand.
- Every control accepts the general node attributes: `id` (unique identifier, used to look the control up in code), `class` (space-separated class list), and `value` (see the per-control sections).
- Every control accepts the style and layout properties from the *DSL User Guide*: size, margin and padding, background, border, corner radius, opacity, `display` / `flex-*` / `grid-*`, and so on. A control's look comes from these properties — there is no appearance that only C++ can change.

### Pseudo-states

There are exactly four, written at the end of a selector:

| Pseudo-state | Trigger |
|---|---|
| `:hover` | the pointer is over this node |
| `:active` | a press is in progress on this node |
| `:focus` | this node holds input focus |
| `:disabled` | the node is disabled |

When several conditions hold at once, one wins in the order `disabled > focus > active > hover`.

All four are available on every control, but only controls that can take focus (`input`) actually enter `:focus`; `:hover` on a container follows hit-testing like any other node.

Disabling is done from C++; there is no DSL attribute for it:

```cpp
control->setEnabled(false);   // disabled: no hover, no press, no focus
control->isEnabled();
```

### Default appearance

The engine ships default styling for some controls (button, input, combobox, text). You can use it as is or override it in `@style`. Overrides take part in normal cascade rules: later rules and more specific selectors win.

### Sizing and content measurement

| Case | Behaviour |
|---|---|
| `width` / `height` set to `auto` or omitted | measured from content: text for `text`, the image's own pixel size for `image` |
| explicit size given | used as given; overflowing content (`text`) is clipped to its own box instead of spilling onto neighbours |
| padding and border | always border-box: `padding` and `border` are inside the size you declare |

### Scrollbars

- Controls that need scrolling create their own scrollbars: multiline inputs (`type="textarea"`) and containers whose content overflows while `overflow` is `auto`/`scroll`.
- Thickness comes from the host node's `scrollbar-width` style property; when absent, a thin scrollbar is used (6 by default).
- Writing a `scrollbar` node by hand is normally unnecessary; if you do need one, create it with `ControlFactory::getInstance()->CreateScrollbar(orientation)` (`ScrollbarControl::Orientation::Vertical` / `Horizontal`).

---

## 3. Control Table

| Tag | Purpose | Own attributes | C++ type | Dispatched events |
|---|---|---|---|---|
| `panel` | container: grouping, cards, layout host | — | `loppe::Panel` | generic mouse events |
| `text` | static text | `value` | `loppe::Text` | — |
| `button` | clickable button (label via a child `text`) | — | `loppe::Button` | `Click`, `MouseDown`, `MouseUp` |
| `input` | text entry (single- or multi-line) | `value`, `placeholder`, `type`, `wrap` | `loppe::Input` | `Input`, `Focus`, `Blur`, `KeyDown`, `KeyUp` |
| `combobox` | dropdown selection | `options` / `arrow` | `loppe::ComboBox` | `Change` |
| `radio` | radio dot | `checked`, `group`, `value` | `loppe::RadioButton` | generic mouse events |
| `progress` | progress bar | `value` | `loppe::ProgressBar` | — |
| `icon` | SVG icon | `name` | `loppe::Icon` | generic mouse events |
| `image` | image | `src` | `loppe::Image` | generic mouse events |
| `scrollbar` | scrollbar (created by the engine) | — | `loppe::ScrollbarControl` | `Scroll` |
| — (no DSL tag) | soft keyboard (engine-managed, for touch text entry) | — | `loppe::SoftKeyboard` | — (writes to the focused input directly) |

"Generic mouse events" means `Click`, `MouseDown`, and `MouseUp`, dispatched by any control that is hit; the semantics are described in *From C++ to UI* §5.

---

## 4. Control by Control

### 4.1 `panel` — container

**Purpose**: group controls together; carries layout (`display: flex` / `grid`), background, border, corner radius, and padding.

**Attributes**: none of its own.

**Default appearance**: no background, no border — you get only what you write.

**Pseudo-states**: `:hover` and `:active` follow hit-testing (when a child is hit, the parent container is on the hit path and events bubble to it); containers do not take focus.

**C++ type**: `loppe::Panel` (a container control). At runtime `setPadding(left, top, right, bottom)` (same order as `AbstractControl::setPadding`) plus `getPaddingTop/Right/Bottom/Left`; padding is normally written in the style rather than set from code.

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
    text(value="Card title", class="card-title"),
    text(value="Card body")
}
```

### 4.2 `text` — text

**Purpose**: display static text; accepts no input.

**Attributes**: `value` (the text).

**Related style properties**: `color`, `font-size`, `font-family`, `font-weight`, `line-height`, `text-align` — these inherit from parent nodes.

**Default appearance**: font size 14, black text, width and height measured from content.

**Behaviour**: with `width: auto` the size follows the text content; when text exceeds its own box it is clipped rather than overlapping neighbours. Line wrapping follows the layout context — `text` has no wrapping attribute. **Single-line text is vertically centred in its own box** (for example `.menu-row { height: 28px }`, where the style fixes the row height); text that wraps to several lines stays top-aligned and is clipped as described above when it does not fit.

A `\t` (tab) in the content is laid out on tab stops: one stop every `4 × font size` from the start of the display row; the tab fills the gap up to the next stop and draws nothing. Use `\t` in `value` instead of runs of spaces when aligning table-like text.

**C++ type**: `loppe::Text`.

| Interface | Notes |
|---|---|
| `setText(std::string)` / `getText()` | set / read the text |
| `setFontSize(float)` / `setFontFamily(std::string)` | size / family |
| `setBold(bool)` / `setItalic(bool)` | weight / italic |
| `setLineHeight(float)` / `setAlignment(...)` | line height / alignment (`Left` `Center` `Right`) |
| `setTextColor(ui::Color)` | text colour |
| `setWrap(bool)` | wrapping (layout decides this; rarely needed by hand) |

```loppe
text:total(class="total", value="12 items")
```

### 4.3 `button` — button

**Purpose**: a clickable container. Put the label in a child `text` node; icons or other children work too.

**Attributes**: none of its own; size, background, border, corner radius, and padding all come from styles.

**Default appearance** (usable as is, overridable)

| Item | Value |
|---|---|
| background | `#007BFF` |
| text / border | `#FFFFFF` / 1px `#0056B3` |
| radius / font size / padding | 6 / 14 / `8 16` |

**Default pseudo-states**: `:hover` → background `#0069D9`; `:active` → background `#0056FF`, border `#003F7F`; `:disabled` → background `#555555`, text `#AAAAAA` (**colour only** — the default rules never touch font-size / padding / radius, so the button geometry stays identical while disabled).

**Events**: `Click` (press and release on the same button), `MouseDown`, `MouseUp`.

**C++ type**: `loppe::Button` (a container control) with no dedicated interface; label and appearance come from child nodes and styles.

```loppe
button:submit(class="btn") {
    text(value="Submit")
}
```

```cpp
onClick("#submit", [](const loppe::UIEventSystem::UIEvent&) { submit(); });
```

### 4.4 `input` — text entry

**Purpose**: a single-line entry field; `type="textarea"` makes it multiline.

**Attributes**

| Attribute | Value | Notes |
|---|---|---|
| `value` | string | initial content |
| `placeholder` | string | shown while the content is empty |
| `type` | `textarea` or anything else | `textarea` goes multiline; other values stay single-line |
| `wrap` | `true` / `false` | whether a multiline input wraps (on by default) |

**Default appearance**: background `#FFFFFF`, text `#000000`, border 1px `#AAAAAA`, radius 4, font size 14, padding `2 4`, **width 200** (write `width` if you want another width).

**Default pseudo-states**: `:focus` → border `#80bdff` plus a halo shadow.

**Behaviour**

- Click to place the caret; double-click selects a word; triple-click selects the whole line; dragging extends the selection.
- Selection, undo / redo, and press-and-hold repeat for Backspace and Delete are supported.
- Tab inserts a tab character (repeat while held); `\t` is laid out on the same tab stops as in `text`.
- Scrollbars appear **only in multiline** mode: vertical when content is taller than the viewport, horizontal when wrapping is off and content is wider than the viewport. Single-line inputs show none and scroll internally to follow the caret.
- There is no password masking in this version: `type` only distinguishes single-line from multiline.

**Events**: `Input` (content changed), `Focus`, `Blur`, `KeyDown`, `KeyUp`.

**C++ type**: `loppe::Input`.

| Interface | Notes |
|---|---|
| `setValue(std::string, notify = true)` / `getValue()` | set / read the content (`notify=false` suppresses the `Input` event) |
| `setPlaceholder(std::string)` | placeholder text |
| `setType(std::string)` / `setWrap(bool)` | single/multi-line, wrapping |
| `selectAll()` / `selection()` | select everything / read the current selection |
| `setTextColor` / `setFontSize` / `setFontFamily` | text appearance |
| `textSearcher()` | find state (query, matches, current match) |
| `onFocus()` / `onBlur()` | focus hooks |

```loppe
input:name(placeholder="Your name"),
input:note(type="textarea", wrap=true, height=120)
```

Content is not written back to the interface description automatically; call `syncToAST(root, true)` when you need to save it (see *From C++ to UI* §6).

### 4.5 `combobox` — dropdown

**Purpose**: open a popup and pick one option.

**Attributes**

| Attribute | Value | Notes |
|---|---|---|
| `options` | string | the option list, **separated by commas** (an option itself cannot contain a comma) |
| `arrow` | icon name | the expander arrow on the right of the trigger; defaults to `chevron-down`, and `arrow=""` draws no arrow |

**Default appearance**: border 1px `#555555`; text colour and size follow styles (and inherit). The text in both the trigger and the popup rows is vertically centred; the arrow is a built-in engine icon (defaults to `chevron-down`; `arrow` also accepts `chevron-up` / `chevron-right` / `chevron-left`), so the host registers nothing.

**With no options**: the popup height is `item count × 30`, so an empty `options` list gives height 0 and a click looks like "nothing happened". Placeholder text only comes from `setPlaceholder` (the `placeholder` attribute is not read); dynamic lists must be pushed with `setOptions` (the `options` attribute only covers static lists).

**Events**: `Change`, with `ev.text` carrying the selected item's text.

**C++ type**: `loppe::ComboBox`.

| Interface | Notes |
|---|---|
| `setOptions(std::vector<std::string>)` | **replaces** the whole list (never appends) and can be called repeatedly to refresh; a still-present previous selection is kept, otherwise the first item is selected |
| `setSelectedIndex(int)` / `getSelectedIndex()` | set / read the selection (`-1` = none) |
| `getOptionCount()` | number of options |
| `isOpen()` / `openPopup()` / `closePopup()` / `togglePopup()` | popup state and control |
| `setPlaceholder(std::string)` | text shown while nothing is selected (default `Select...`) |
| `setOnWillOpen(std::function<void()>)` | called just before the popup opens — a good place to refresh options (hot-plugged device lists) |
| `setArrowIcon(std::string)` / `getArrowIcon()` | set / read the icon name of the expander arrow (empty = no arrow) |

```loppe
combobox:city(options="Beijing,Shanghai,Guangzhou,Shenzhen")
```

```cpp
onChange("#city", [](const loppe::UIEventSystem::UIEvent& e) {
    const std::string picked = e.text;
});
```

### 4.6 `radio` — radio button

**Purpose**: a selectable dot.

**Attributes**

| Attribute | Value | Notes |
|---|---|---|
| `checked` | `true` / `false` | initially selected |
| `group` | string | group name |
| `value` | string | option label |

**Default appearance**: a circle of radius 8 with a 2-wide stroke; hollow when unselected, filled with a radius-4 dot when selected; the stroke brightens on hover; the pointer becomes a hand over the control.

**Behaviour**: clicking toggles this control's own checked state. **Automatic exclusivity within a group is not implemented in this version**: `group` is stored but has no effect, so clear the other members yourself in an `onClick` callback; `value` only stores the label and is not painted, so place a `text` node next to the radio if you need visible text.

**C++ type**: `loppe::RadioButton`.

| Interface | Notes |
|---|---|
| `setChecked(bool)` / `isChecked()` | set / read the checked state |
| `setGroup(std::string)` / `getGroup()` | group name |
| `setText(std::string)` | option label |

No dedicated events; capture clicks with the generic `onClick`.

```loppe
panel(class="choice") {
    radio:r1(group="plan", value="Basic", checked=true),
    text(value="Basic")
}
```

```cpp
// maintain exclusivity yourself when you need it
onClick("#r1", [this](const loppe::UIEventSystem::UIEvent&) { selectPlan("r1"); });
```

### 4.7 `progress` — progress bar

**Purpose**: show progress from 0 to 1. The track is the control's own background (`background-color`, `border-radius`); the fill is drawn by the control.

**Attributes**: `value` (declared progress, 0–1; out-of-range is clamped to 0 / 1). With it the bar starts at that ratio; without it the value starts at 0 and progress is set by the host. The fill colour is still set from C++.

**Default appearance**: fill colour is blue (about `0.23, 0.51, 0.96`, i.e. `rgb(59,130,245)`); corner radius is clamped to half the width / half the height.

**C++ type**: `loppe::ProgressBar`.

| Interface | Notes |
|---|---|
| `setProgress(float)` / `getProgress()` | progress 0–1; out-of-range values are clamped, equal values do not disturb the frame |
| `setBarColor(ui::Color)` / `getBarColor()` | fill colour |

No events. The fill is drawn by the control — **do not draw it again in a host render callback**.

```loppe
progress:bar(class="bar", value=0.42) { }   // static ratio: write it into the description file
progress:sync(class="bar") { }              // driven at runtime: no value, the host sets it
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

How `value` relates to `setProgress`: once the host has changed the progress, the control keeps the host value;
only a change of the declared value itself (editing the number in the `.lui` and re-parsing) is re-applied —
resizing or re-layout never snaps the runtime progress back to the declared value.

### 4.8 `icon` — SVG icon

**Purpose**: display a registered SVG icon tinted by the current `color`.

**Attributes**: `name` — the icon's registered name.

**Registration**: the registry is filled by the application — register with SVG text at startup and reference it by name in the interface. Registering the same name again overwrites; content without `<svg` is rejected. At startup (`Application::run`) the engine already registers `chevron-down` / `chevron-up` / `chevron-right` / `chevron-left`; any other name is yours to register (unregistered = nothing is drawn).

```cpp
loppe::svg::RegisterIcon("close", "<svg viewBox=\"0 0 16 16\">…</svg>");
loppe::svg::HasIcon("close");   // check whether a name is registered
```

**Default appearance**: sized by `width` / `height` (the icon's own size otherwise); colour follows `color`, so `icon:hover { color: … }` works directly.

**C++ type**: `loppe::Icon`: `setName(std::string)` / `getName()`.

```loppe
icon:closeIcon(name="close", class="icon")
```

### 4.9 `image` — image

**Purpose**: display a local image file.

**Attributes**: `src` — the image path.

**Path resolution** (both forms are common)

```loppe
image(src="~/img/logo.png")   /* relative to the directory of the current .lui */
image(src="pics/photo.jpg")   /* relative to the resource root */
```

- `~/` requires the host to set the "current interface file directory"; see *From C++ to UI* §4.
- Other relative paths are searched under the resource root, then the executable directory.
- Local file paths only; network URLs are not supported. `background-image` follows the same rules.

**Size**: with no `width` / `height` the image's own pixel size feeds measurement; supply them and that size is used instead.

**Loading**: asynchronous. The engine schedules a repaint when loading finishes; the host does not poll.

**C++ type**: `loppe::Image`: `setSrc(std::string)` / `getSrc()` (changing the source at runtime reloads it).

```loppe
image:avatar(src="~/img/avatar.png", class="avatar")
```

### 4.10 `scrollbar` — scrollbar

**Purpose**: indicate and drive scrolling for controls that need it.

**Who creates it**: the engine, on demand; normally not written in an interface file. Triggers are multiline inputs and containers with `overflow: auto` or `scroll` whose content overflows.

**Thickness**: the host node's `scrollbar-width` style property; the global default thickness (8) is used when absent.

**The unit is dp — not px, and not sp**: layout, painting and hit-testing all use dp and are scaled by the
platform `ContentScale` at render time. So "8" is 8 physical pixels on a 100%-scale desktop, 12 at 150%,
and 16 on Android xhdpi. Do not use Android's sp for scrollbars — sp = dp × system font scale, and a
scrollbar that follows the font setting makes control widths jump when the user changes text size.

**Per-platform defaults**: `ScrollbarControl::setDefaultThickness(dp)` is a process-wide runtime default;
set it once at startup, before building the UI (`<= 0` or NaN restores the built-in 8):

```cpp
// desktop: 8dp unless you say otherwise
// Android: touch targets feel small, go a bit thicker than desktop
loppe::ScrollbarControl::setDefaultThickness(12.0f);
```

Priority: instance `setBarThickness()` > node style `scrollbar-width` > global default (`defaultThickness()`).
Layout reservation reads the same global default, so "reserved = painted = hit/drag" always holds.

**Always-reserved lane**: with `scrollbar-gutter: stable` on the container, a scrolling axis permanently reserves one bar width (that reservation is the lane) whether or not the content overflows; X and Y are independent and only `overflow` axes of `auto` / `scroll` qualify. The bar itself is still painted only when the axis can actually scroll. Values and semantics: user guide §6.4.

**Colors**: idle is `thumbColor()` (a pale translucent grey); while the pointer rests on the thumb it uses `thumbHoverColor()`, which defaults to `EmphasizeColor(thumbColor())` — **same RGB, higher opacity** (α → α + (1−α)×0.72). Higher opacity means more contrast against whatever is behind, so the thumb gets brighter on dark backgrounds and darker on light ones: one default works on both themes. Overriding only `thumbColor()` is enough; override both to take full control.

**C++ type**: `loppe::ScrollbarControl`.

| Interface | Notes |
|---|---|
| `ScrollbarControl::Orientation::Vertical` / `Horizontal` | orientation |
| `kScrollbarThickness` | built-in baseline constant (8dp; used when `setDefaultThickness()` was never called) |
| `setDefaultThickness(dp)` / `defaultThickness()` | runtime override / read of the global default |
| `thumbColor()` / `thumbHoverColor()` | idle / hovered thumb color (protected virtual; override to recolor) |
| `EmphasizeColor(color)` | hover emphasis: same RGB, higher opacity; near-opaque inputs shift RGB by luminance instead |
| `ControlFactory::getInstance()->CreateScrollbar(orientation)` | create one manually |

```loppe
@style {
    .list { overflow: auto; scrollbar-width: 8; }
}
```

---

### 4.11 Soft keyboard (`loppe::SoftKeyboard`)

**Purpose**: on-screen keyboard for touch text entry. An engine-managed panel pinned to the bottom of the screen; it is not part of the layout tree and is drawn on top. One implementation for all platforms (desktop / Android / embedded), rendered through generic DisplayList commands.

**Behavior** (all on by default, zero application code):

| Behavior | Description |
|---|---|
| Auto show on focus | when an `input` gains focus (touch input source) → shown; when focus leaves the input → hidden |
| No touch pass-through | touches on the keyboard are consumed by it; they neither change focus nor hit the UI beneath |
| Keys go straight to the input | key presses are delivered to the focused input (same path as a physical keyboard; `Input` events fire normally) |
| ⇧ / ?123 / DEL | case toggle (reverts to lowercase after one letter), letters and numeric/symbol layers, long-press DEL auto-repeat |

**C++ API** (get the instance via `Scene::softKeyboard()`; the keyboard is created lazily):

| API | Description |
|---|---|
| `show()` / `hide()` / `isVisible()` | manual show / hide (while shown manually, auto-hide is suspended until `hide()`) |
| `setEnabled(bool)` | master switch (when off: never shown, never auto-pops) |
| `setAutoShow(bool)` | disable focus-based auto show; only `show()/hide()` control it |
| `setFontFamily(string)` | key label font (empty = platform default) |
| `panelHeight()` | keyboard panel height (scales with screen width) |

**Notes**:

- Desktop (mouse input source) never auto-pops; call `scene->softKeyboard()->show()` if needed.
- Screen size is synced automatically by `ViewCoordinator` (follows window / resolution changes); hosts do not manage it.
- Embedded: `EmbeddedHost` needs no wiring at all — focusing an `input` pops the keyboard.

---

## 5. Events per Control

| Event | Meaning | Dispatched by |
|---|---|---|
| `Click` | press and release on the same control | any hit-testable control |
| `MouseDown` / `MouseUp` | mouse button pressed / released | any hit-testable control |
| `Input` | content changed | `input` |
| `Change` | options or selection changed | `combobox` |
| `Scroll` | wheel scrolling | scroll containers, scrollbars |
| `Focus` / `Blur` | input focus gained / lost | `input` |
| `KeyDown` / `KeyUp` | key pressed / released | the focused control |

Binding syntax, bubbling, and stopping propagation are covered in *From C++ to UI* §5.

---

## 6. Known Limitations and Common Problems

| Symptom | Cause | Fix |
|---|---|---|
| No text on a button | the label must be a child `text` node; a button has no text attribute | put `text(value="…")` inside `button { }` |
| `input` always ends up a fixed width | the engine's default style sets `width: 200` | write an explicit `width` in `@style` |
| A single-line input has no scrollbar | single-line inputs never show one | use `type="textarea"` if you need a scrollbar |
| Two `radio` controls are selected at once | group exclusivity is not implemented yet | clear the other members in an `onClick` callback |
| No label beside a `radio` | `value` is not painted | add a `text` node next to the radio |
| A `combobox` option gets split | `options` is split on commas | avoid commas inside option text, or pass a list through `setOptions` from C++ |
| An `icon` shows nothing | its `name` was never registered | call `svg::RegisterIcon(name, svgText)` at startup |
| An image shows nothing | the path is neither under the `.lui` directory, the resource root, nor the executable directory | use the `~/` prefix or adjust the resource root; confirm the file exists |
| A disabled control still reacts to clicks | disabling must be applied to the control | call `setEnabled(false)`; disabled nodes never enter hover / press / focus |
| The progress bar is drawn twice | the host drew the fill as well | only call `setProgress`; do not re-draw in a render callback |

---

## Related Documents

| Document | Contents |
|---|---|
| `LOPPE_GETTING_STARTED_EN.md` | from an empty C++ project to display, interaction, and runtime updates |
| `LOPPE_CLI_GUIDE_EN.md` | building / validating with `loppe-cli`, finding output, reading build diagnostics |
| `LOPPE_MULTI_UI_EN.md` | several `.lui` files / several screens in one project: mount points, mounting and unmounting, shared styles |
| `LOPPE_DSL_USER_GUIDE_EN.md` | DSL syntax, selectors, every style and layout property |
| `LAYOUT_DESIGN_EN.md` | size model, box model, flex / grid space allocation |
| `LOPPE_CUSTOM_CONTROL_GUIDE_EN.md` | implementing controls the engine does not ship |
| `ai/LOPPE_DSL_AI_REFERENCE_EN.md` | full property tables and differences from standard CSS (for AI use) |
