# LOPPE Custom Control User Guide

> 📖 [中文版](LOPPE_CUSTOM_CONTROL_GUIDE.md) | English

---

## 1. What This Document Covers

A custom control is a UI element that the engine's built-in tags do not cover. This document explains how to implement one in your own project. There are three approaches:

| Approach | How it works | C++ required | APIs used |
|---|---|---|---|
| **1. Compose existing controls** | Write a DSL template that assembles existing controls into a new one | No, only the mount call | `UIComponent`, `ComponentInstantiator`, `ElementView` |
| **2. Self-drawn control type** | Add a new DSL tag whose drawing and interaction live in your own C++ class | Yes | `AbstractControl` subclass + `ControlFactory` subclass |
| **3. Appearance only** | Keep the structure and behaviour of existing controls, change only styles and states | No | `@style`, `@layout`, classes, pseudo-state selectors |

The three approaches compose: a control built with approach 2 can use an approach-1 DSL template for its inner skeleton, and its appearance follows approach 3.

### 1.1 Who This Is For

Application developers building UIs with the LOPPE UI engine.

### 1.2 Prerequisites

- The engine package is wired into your project: `lib/*.a` + `include/*.h`, with `${pkg}/include` in the include list.
- You can write `.lui` UIs. File structure, selectors, style and layout properties are covered in `LOPPE_DSL_USER_GUIDE.md`.
- You organise your project's source list and include directories with `build.lpj`.
- In this document, the **host** is your own project: the side that owns the `Scene`, listens to events and holds the application logic.

### 1.3 Choosing an Approach

Answer two questions:

1. Can the control be assembled from existing controls (`panel` + a few children + classes)?
   - Yes → approach 1. If only the look differs, use approach 3 and skip C++ entirely.
   - No → ask the second question.
2. Do you need to draw pixels yourself, compute your own layout, or take over the keyboard and the wheel?
   - Yes → approach 2.
   - No → approach 3.

---

## 2. How to Implement a Custom Control by Composing Existing Controls (Approach 1)

Fits controls that are a composition of existing ones: statistic cards, list rows, navigation blocks.

### 2.1 Step 1: Define the Component Shell

```cpp
#include "components/ui_component.h"

class StatCard : public loppe::UIComponent {
public:
    void onAttach(loppe::Scene* scene, const std::string& parentId) override;
    // onDetach(scene) defaults to a no-op; override it to unbind host callbacks or stop worker threads
};
```

`UIComponent` provides only the `onAttach` / `onDetach` pair and holds no other logic; the mounting itself is done by the API in §2.3.

### 2.2 Step 2: Write the DSL Template

```cpp
#include "engine/core/ast_snippet.h"
#include "engine/core/parse.h"

static const loppe::AstSnippet kStatCard = []{
    loppe::LoppeParser parser;
    return parser.parse(R"ui(
@style   { .stat-card { background-color:#1f2733; border-radius:8px; padding:12px; } }
@layout  { .stat-card { display:flex; flex-direction:column; } }
panel(class="stat-card") {
    text(class="stat-title", value="Downloads"),
    text(class="stat-value", value="0")
}
)ui");
}();
```

Two requirements for the template:

- **Parse it once.** An `AstSnippet` can be instantiated repeatedly — every mount takes its own deep copy and instances never interfere — so a `static` parse followed by N mounts is safe. List rows must work this way; never re-parse on every mount.
- **No `id`.** Instantiating the same template twice collides on `id`; locate elements by class inside the template and inject a unique id from the caller, or call `setId` after mounting.

### 2.3 Step 3: Mount the Template Under a Parent Node

```cpp
#include "components/component_instantiator.h"

void StatCard::onAttach(loppe::Scene* scene, const std::string& parentId) {
    if (!scene) return;

    loppe::ElementView root = loppe::ComponentInstantiator::SpawnElementViewUnderId(
        scene, parentId, kStatCard, scene->getDocumentRuntime());
    if (!root) return;                       // failure: parentId missing, or the template failed to parse
}
```

Four overloads = `{DSL string, AstSnippet}` × `{mount the first top-level node, mount all top-level nodes}`:

```cpp
static ElementView              SpawnElementViewUnderId (Scene*, const std::string& parentId, const std::string& dsl, DocumentRuntime*, int index = -1, ResolvedRuleScope = Local);
static ElementView              SpawnElementViewUnderId (Scene*, const std::string& parentId, const AstSnippet&,      DocumentRuntime*, int index = -1, ResolvedRuleScope = Local);
static std::vector<ElementView> SpawnElementViewsUnderId(Scene*, const std::string& parentId, const std::string& dsl, DocumentRuntime*, int index = -1, ResolvedRuleScope = Local);
static std::vector<ElementView> SpawnElementViewsUnderId(Scene*, const std::string& parentId, const AstSnippet&,      DocumentRuntime*, int index = -1, ResolvedRuleScope = Local);
```

- The return value is the success signal: `if (!view)` or `views.empty()`. There is no separate `bool` API.
- `index` is where the first top-level node goes among the parent's children: `-1` appends, `0` inserts first.
- `ResolvedRuleScope` controls the scope of the template's own rules — see §2.7.

### 2.4 Step 4: Operate on Mounted Elements with ElementView

The handle holds only a `weak_ptr` and invalidates itself when the element is removed, so it never dangles:

| Member | Meaning |
|---|---|
| `valid()` / `operator bool` | Is the element still alive |
| `control()` | The underlying `AbstractControl*`; `dynamic_cast` back for your own API |
| `id()` / `uid()` | Identity |
| `addClass` / `removeClass` / `hasClass` | Switch state (preferred: let style rules own the appearance) |
| `remove()` | Detach from the parent; the structural change applies this frame, memory is reclaimed at frame end |
| `ancestorByClass(cls)` / `descendantByClass(cls)` | Row/element lookup for event delegation |
| `ElementView::fromControl(ctrl)` | Recover the handle from `UIEvent::target` |

### 2.5 Step 5: Update Data

Incremental alignment for list-like controls: remove only the rows that must close, append only new rows, toggle a class on the rows that stay:

```cpp
for (auto& r : rows) if (needClose(r)) { r.view.remove(); r.view = {}; }
for (auto& item : newItems)
    rows.push_back({ item, loppe::ComponentInstantiator::SpawnElementViewUnderId(scene, "list", kRow, ctx) });
```

Deleting everything and rebuilding is a structural edit: it re-parses the DSL, re-registers rules and rebuilds the control tree. Measured on a 20-row list (Release):

| Refresh strategy | Time per switch |
|---|---|
| Delete-and-rebuild | ≈3.3 ms |
| Pre-built rows, text/class updates only | ≈0.96 ms |

When the row count does not change, reuse rows and update data or classes; add and remove elements only when the count really changes.

### 2.6 Step 6: Handle State and Interaction

- Express state as classes: the host calls `addClass("active")` / `removeClass("active")`, and the style block has `.stat-card.active { ... }`.
- Use the existing pseudo-state selectors for interaction: `:hover`, `:active`, `:focus`, `:disabled`.
- If a class change affects size, use `addClass(cls, /*forceLayout*/ true)`, or let the parent re-layout afterwards.

### 2.7 Step 7: Choose the Scope of the Template's Rules

The last parameter of `SpawnElementView*` decides how the template's own `@style` / `@layout` rules are registered:

- `ResolvedRuleScope::Local` (default): registered as **per-instance** rules; instances do not interfere.
- `ResolvedRuleScope::Global`: registered document-globally, for splitting one CSS set across files or sharing it between repeated mounts.

Stay on `Local` unless a CSS set genuinely has to be shared across several mounts and you are sure the selectors will not bleed.

---

## 3. How to Implement a Custom Control as a Self-Drawn Control Type (Approach 2)

Fits controls that draw their own pixels, compute their own layout, or take over the keyboard and the wheel: code editors, tree views, rich text. The implementation is one `AbstractControl` subclass plus one `ControlFactory` subclass, in eleven steps.

### 3.1 Step 1: Define the Control Class

```cpp
#pragma once
#include <components/abstract_control.h>

namespace myapp {

class Dot : public loppe::AbstractControl {
public:
    std::string getTypeName() const override { return "dot"; }   // must equal the DSL tag name

protected:
    void onRender(loppe::DisplayList& dl, const loppe::ui::Rect& clip) const override {
        (void)clip;
        dl.addRect(bounds_, getFillColor(), getStrokeColor(), getStrokeWidth(), getCornerRadius());
    }
};

} // namespace myapp
```

- `onRender` and `getTypeName` are pure virtual and must be implemented; every other virtual has a default, override as needed.
- `getTypeName()` contract: case-insensitively equal to the DSL element type name (the `dot` in `dot:foo`). Never return the C++ class name — type selectors such as `dot { ... }` match on it.
- The `clip` passed to `onRender` is the clip region accumulated down the ancestor chain (in this control's local coordinates); pass it through to your draw calls.

Overridable hooks and when they run:

| Member | When | Responsibility |
|---|---|---|
| `onRender(dl, clip)` | Every frame (pure virtual) | Draw only; never mark dirty or rebuild layout here |
| `getTypeName()` | Any time (pure virtual) | Return the DSL tag name |
| `isContainer()` | Control-tree build | Return `true` to have children built as child controls (containers only) |
| `OnSyncFromResolvedNode(rn)` | After the generic mapping (and after every layout) | Sync derived properties (see §3.4) |
| `onLayoutSynced()` | After bounds / viewport / scroll sync | Update your layout caches for the new size (see §3.5) |
| `handleLocalMouseButton/Move/Scroll(e)` | When hit-testing reaches this control | Hit-test and drag in local coordinates (see §3.6) |
| `onKey(e)` / `onChar(e)` | While this control holds focus | Return `true` when handled |
| `onFocus()` / `onBlur()` | Focus changes | Update your state, reset blink timers, … |
| `onHover(bool)` / `onActive(bool)` | Pointer hover / press | State and repaint only; nothing heavy |
| `update()` | Frames this control was queued for | Time-driven logic (see §3.8) |
| `onFrameCommit()` | After this frame's update, before paint | Catch up internal dirty layout produced this frame (see §3.8) |
| `invalidate()` | You call it | "I changed": mark dirty + request repaint + keep ticking next frame |
| `requestAnimationTick()` | You call it | Ask for a tick next frame only; no dirty, no repaint |
| `markLayerDirty()` | You call it | Mark this control dirty only (visual change that needs no continued ticking) |
| `setScrollSize(w,h)` | Layout writes the scroll size | Self-managed scrollers override it as a no-op and compute their own (see §3.7) |
| `setViewportSize(w,h)` | Layout writes the viewport | Usually the base implementation; call `AbstractControl::setViewportSize` after computing your own |

### 3.2 Step 2: Implement Drawing

`DisplayList` provides:

```cpp
dl.addRect(rect, fill, stroke = {}, strokeWidths = {}, cornerRadius = 0.0f);
dl.addText(text, rect, color, fontSize = 0.0f, fontFamily = "", layout = TextLayoutParams());
dl.addTextLayout(layout, rect, color, firstLine = 0, lineCount = 0);   // when you lay out text yourself
dl.addImage(source, rect);
dl.addSvgIcon(name, rect, tint = white);      // name = a registered icon name
dl.pushClip(rect /*, cornerRadius */); ... dl.popClip();
dl.pushTransform(dx, dy); ... dl.popTransform();
```

- `bounds_` is this control's rect in parent coordinates; compute your inner geometry from it.
- For self-managed scrolling, `pushTransform(-scrollX, -scrollY)` before drawing the content and `pushClip` to your viewport, or the content paints over neighbouring controls.
- On the first frame, or while layout is not ready, paint the background and return; do not build layout here.

### 3.3 Step 3: Register the Intrinsic Size

When the DSL does not fix width/height, the layout engine looks the intrinsic measurement up by node type, and that table is open:

```cpp
#include <engine/pipeline/content_measure_registry.h>

static void ensureDotMeasureRegistered() {
    static bool once = false;
    if (once) return;
    once = true;

    loppe::ContentMeasureRegistry::instance().registerType(
        "dot",
        [](loppe::ResolvedNode*, const loppe::ContentMeasureRegistry::Context& ctx,
           float& requestedW, float& requestedH, float /*availableWidth*/) {
            float w = 12.0f, h = 12.0f;                 // the intrinsic default for this type
            if (ctx.textMeasurer) { /* measure text content with ctx.textMeasurer */ }
            if (requestedW == loppe::AUTO_SENTINEL) requestedW = w;   // fill only dimensions still at AUTO
            if (requestedH == loppe::AUTO_SENTINEL) requestedH = h;
        });
}
```

- Fill only dimensions still at `AUTO_SENTINEL`: an explicit `width` / `height` belongs to the style, do not override it.
- Registering is optional: without it, write explicit sizes in the DSL or let the defaults apply.
- Call `ensureDotMeasureRegistered()` once from the constructor (`static bool once` makes it idempotent).

### 3.4 Step 4: Sync DSL Attributes and Style Properties

After every layout or change, the engine runs the generic mapping first (bounds, scroll, viewport, `background-color`, `border`, `border-radius`, `padding`, `visible`, `background-image`) and then calls your `OnSyncFromResolvedNode(rn)`:

```cpp
void Dot::OnSyncFromResolvedNode(const loppe::ResolvedNode& rn) {
    // (1) DSL attributes: dot(id="x", tone="danger", value="3")
    if (rn.astNode) {
        auto it = rn.astNode->attrs.find("tone");
        if (it != rn.astNode->attrs.end() && it->second.isString())
            tone_ = it->second.as<std::string>();
    }
    // (2) Style properties: the final computed values from @style / classes
    const float fs = rn.getFinalStyle().fontSize;
    if (fs > 0.0f && fs != fontSize_) { fontSize_ = fs; layoutDirty_ = true; markLayerDirty(); }
}
```

Three requirements:

1. **Apply initial values once**: for attributes the host or the user may change at runtime (`value`), guard with a `bool initialized_` flag, or the next style sync overwrites what you already changed.
2. **Mark dirty when a size-affecting derived property changes** (`layoutDirty_ = true` + `markLayerDirty()`), or the picture will not follow.
3. **Do not re-parse generic appearance**: background, border, radius, padding and visibility are already mapped by the base class into `getFillColor()` / `getStrokeColor()` / `getStrokeWidth()` / `getCornerRadius()` / `getPadding*()` / `isVisible()`; consume them in `onRender`.

### 3.5 Step 5: Update Caches in the Layout Hook

`onLayoutSynced()` runs after **every** layout sync (the engine walks all controls each frame), so:

- Do: convert the new size into your caches (line height, viewport, scrollbar geometry, content size).
- Do not rebuild unconditionally: bail out early when `layoutDirty_` is clean. The unconditional version is expensive — it re-measures the whole document on every splitter drag (230 lines ≈ 200 ms/frame, 1500 lines ≈ 1.35 s/frame).
- If the new size really does affect your internal layout, mark dirty only when the size really changed, not every frame.

### 3.6 Step 6: Handle Mouse and Keyboard Input

```cpp
bool Dot::handleLocalMouseButton(const loppe::ControlLocalMouseButtonEvent& e) {
    if (!isVisible()) return false;
    if (e.action == GLFW_PRESS) {
        if (!hitTestLocal(e.point)) return false;
        toggle();                     // your own logic
        captureMouse();               // capture when dragging/selecting; releaseMouse() afterwards
        invalidate();
        return true;
    }
    if (e.action == GLFW_RELEASE) { releaseMouse(); return true; }
    return false;
}
```

- Event points are already in this control's local coordinates (the engine did the conversion); test "is the point on me" with `hitTestLocal(point)`.
- To keep receiving events after the pointer leaves (drag, selection), use `captureMouse()` / `releaseMouse()`; use `captureMouse(relativeTo)` for a different reference frame.
- The keyboard arrives through `onKey` / `onChar` and only while focused; `onChar` is text input, `onKey` is function and modifier keys.
- Change the pointer shape with `getScene()->setCursor(Scene::Cursor_Hand /* or Arrow / IBeam */)`.

### 3.7 Step 7: Manage Scrolling Yourself (When Needed)

When the control decides its own scroll range (code editors, self-drawn lists), override `setScrollSize` as a no-op and compute the viewport and content sizes yourself:

```cpp
// In the declaration: ignore the content size handed in by layout
void setScrollSize(float w, float h) override { (void)w; (void)h; }

void MyView::applyScrollMetrics() {
    float vw = 0.0f, vh = 0.0f;
    textViewportSize(vw, vh);                            // your content area (scrollbar gutters already deducted)
    setViewportSize(vw, vh);                             // viewport = the visible window

    // Qualify the base class to bypass the no-op override and publish your own content size
    loppe::AbstractControl::setScrollSize(contentW_, contentH_);

    // After the content shrank or the data changed, clamp out-of-range offsets
    const float nx = std::max(0.0f, std::min(getScrollX(), getScrollWidth()  - vw));
    const float ny = std::max(0.0f, std::min(getScrollY(), getScrollHeight() - vh));
    if (nx != getScrollX() || ny != getScrollY()) { setScroll(nx, ny); markLayerDirty(); }
}
```

Scrolling API on the base class: `setScroll(x,y)` / `getScrollX|Y()`, `setScrollSize(w,h)` / `getScrollWidth|Height()`, `setViewportSize(w,h)` / `getViewportWidth|Height()` / `hasViewport()`, and `styleScrollbarWidth()` (reads the node's current `scrollbar-width`).

`hasViewport()` is true only when `overflow != visible`; do not use it as a clip switch — clipping in `onRender` is governed by `clip`.

### 3.8 Step 8: Repaint and Time-Driven Work

| Situation | Use |
|---|---|
| Appearance changed, one-shot repaint | `markLayerDirty()` (this control only) or `invalidate()` (dirty + repaint request + tick next frame) |
| Time-driven work (caret blink, key repeat) | Decide inside `update()`; to continue, `requestAnimationTick()`, and `invalidate()` only when it actually changed |
| Content or state changed, the host must know | Report from `update()` after de-duplicating (see §3.9) |

Two common mistakes:

- Calling `invalidate()` every frame: that is a full-screen repaint every frame.
- Calling only `markLayerDirty()` without `requestAnimationTick()`: the control is not queued into that frame's `update()`, so "report scroll position to the host" never fires (this is how two-pane synced scrolling breaks).

Internal dirty layout produced inside `update()` is caught up in `onFrameCommit()` (it runs after update, before paint); without it, `onRender` takes its "layout not ready" early-out and the frame looks dropped or flickering.

### 3.9 Step 9: Report Events to the Host

Dispatch an event after an internal state change; the host listens by selector:

```cpp
void Dot::emitChange() {
    if (!getScene()) return;
    auto* ui = getScene()->getUIEventSystem();
    if (!ui) return;
    loppe::UIEventSystem::UIEvent e;
    e.type   = loppe::UIEventSystem::UIEventType::Change;   // or Input / Click / …
    e.target = this;
    e.text   = std::to_string(count_);
    ui->dispatch(e);
}
```

Host side (selectors support `#id` and `.class`):

```cpp
scene->onChange("#unread", [](loppe::UIEventSystem::UIEvent& e) { /* e.text = new value */ });
scene->onClick(".row",    [](loppe::UIEventSystem::UIEvent& e) { /* e.deepestTarget = the control actually clicked */ });
scene->onInput("#code",   cb);
scene->onScroll("#list",  cb);
```

`UIEventType` values: `Click`, `MouseDown`, `MouseUp`, `Input`, `Change`, `Scroll`, `Focus`, `Blur`, `KeyDown`, `KeyUp`.

- `e.target` is the control currently being dispatched to (DOM `currentTarget`); `e.deepestTarget` is the deepest hit control (DOM `target`).
- Handlers run along the hit path from the deepest node upward; setting `e.stopPropagation = true` stops propagation (later handlers on the same node do not run either).
- Coalesce high-frequency events yourself: compare against the previously reported value inside `update()` and dispatch only on change.
- **Threading**: control APIs are UI-thread only. Background threads (downloads, git, indexing) must `UiDispatcher::post(...)` back to the UI thread before touching controls.

### 3.10 Step 10: Define the Factory and Install It

Something has to turn the `dot` tag into your `Dot` object:

```cpp
#pragma once
#include <components/control_factory.h>
#include <engine/core/ast_node.h>

namespace myapp {
class MyControlFactory : public loppe::ControlFactory {
public:
    std::shared_ptr<loppe::AbstractControl> doMakeControlFromResolvedNodeHook(loppe::ResolvedNode* rn) override {
        if (!rn || !rn->astNode) return nullptr;
        if (rn->astNode->type == "dot") return std::make_shared<Dot>();
        return nullptr;      // nullptr = fall back to built-in tags (panel/text/input… keep working)
    }
};
} // namespace myapp
```

`doMakeControlFromResolvedNodeHook` is the one entry point for new DSL tags: the engine asks it first and falls back to the default implementation of the built-in tags (`panel` / `text` / `button` / `input` / `combobox` / `radio` / `progress` / `icon` / `image` / `Root`) when it returns null.

Install it once, before documents are parsed and controls are created (host startup), for example in `Application::build()`:

```cpp
loppe::ControlFactory::setInstance(std::make_unique<myapp::MyControlFactory>());
```

- Installing too late (the control tree is already built) leaves those nodes without a control and the UI blank.
- One install per process is enough; when integration tests must swap the factory temporarily, pair `ControlFactory::releaseInstance()` with `setInstance(...)` and restore afterwards.

One project can host several custom controls: they all go through the same factory, and the hook branches on `rn->astNode->type`. To add your types on top of an existing custom set, take a base factory in the constructor and delegate:

```cpp
class MyFactory : public loppe::ControlFactory {
public:
    explicit MyFactory(std::unique_ptr<loppe::ControlFactory> base) : base_(std::move(base)) {}

    std::shared_ptr<loppe::AbstractControl> doMakeControlFromResolvedNodeHook(loppe::ResolvedNode* rn) override {
        if (rn && rn->astNode && rn->astNode->type == "dot") return std::make_shared<Dot>();
        if (base_) return base_->doMakeControlFromResolvedNodeHook(rn);   // ask the wrapped layer first
        return nullptr;                                                   // then fall back to built-in tags
    }
private:
    std::unique_ptr<loppe::ControlFactory> base_;
};
```

### 3.11 Step 11: Restyle Engine-Created Scrollbars (Optional)

```cpp
std::shared_ptr<loppe::ScrollbarControl> CreateScrollbar(loppe::ScrollbarControl::Orientation o) override {
    return std::make_shared<MyThinScrollbar>(o);   // must derive from ScrollbarControl
}
```

Thickness is read live by the scrollbar from the host node's `scrollbar-width` style; no host wiring is needed.

### 3.12 Owned Child Controls (When You Need Private Helpers)

Helper controls a control owns — scrollbars being the classic case: hold them in a `shared_ptr`, set yourself as their parent, inject the scene, and forward events yourself (they are not in the layout tree, so the hit path does not include them):

```cpp
#include <components/scrollbar_control.h>   // loppe::ScrollbarControl

void ensureScrollbars() {
    if (!sbV_) { sbV_ = std::make_shared<MyScrollbar>(loppe::ScrollbarControl::Orientation::Vertical);
                 sbV_->setParent(this); }
    if (getScene() && sbV_->getScene() != getScene()) sbV_->setScene(getScene());
}
```

- Hit-test with parent-local points (`makeParentLocalPoint(point.x.value, point.y.value)`), the same frame the bar's own lane rect uses.
- Forward mouse moves to the bar manually (`onParentLocalMouseMove`) and clear its hover state when the pointer leaves your control.

---

## 4. How to Implement a Custom Control by Changing Only Its Appearance (Approach 3)

Fits controls whose structure and behaviour already work and only the look or the states must change.

1. **Build the structure**: use `panel` / `text` / `icon` / `image` (or an approach-1 DSL template), or use an existing control directly.
2. **Express state as classes**: the host calls `addClass("active")` / `removeClass("active")`, and the style block has `.stat-card.active { ... }`.
3. **Use pseudo-state selectors for interaction**: `:hover`, `:active`, `:focus`, `:disabled`.
4. **Use registered SVG names for icons** (`icon(name="...")`); register new icons first.
5. **If a class change affects size**, use `addClass(cls, /*forceLayout*/ true)`, or let the parent re-layout afterwards.

Appearance stays in the `.lui` file and style blocks, so theming, i18n and re-skins need no recompilation.

---

## 5. Complete Example: a Self-Drawn `badge`

**1) The control** (`components/badge.h` / `badge.cpp`)

```cpp
// badge.h
#pragma once
#include <components/abstract_control.h>
#include <string>

namespace myapp {

// Self-drawn badge: rounded background + centred number; a click dispatches a Click event to the host.
class Badge : public loppe::AbstractControl {
public:
    std::string getTypeName() const override { return "badge"; }

    int value() const { return value_; }
    void setValue(int v) { if (v == value_) return; value_ = v; layoutDirty_ = true; markLayerDirty(); }

    static void ensureMeasureRegistered();   // intrinsic size

protected:
    void onRender(loppe::DisplayList& dl, const loppe::ui::Rect& clip) const override;
    void OnSyncFromResolvedNode(const loppe::ResolvedNode& rn) override;
    void onLayoutSynced() override { layoutDirty_ = false; }
    bool handleLocalMouseButton(const loppe::ControlLocalMouseButtonEvent& e) override;

private:
    int  value_ = 0;
    bool initialized_ = false;
    mutable bool layoutDirty_ = true;
};
} // namespace myapp
```

```cpp
// badge.cpp
#include "badge.h"
#include <engine/pipeline/content_measure_registry.h>
#include <engine/core/ast_node.h>
#include <engine/core/resolved_node.h>
#include <engine/event/ui_event_system.h>
#include <components/scene.h>

namespace myapp {

void Badge::ensureMeasureRegistered() {
    static bool once = false;
    if (once) return;
    once = true;
    loppe::ContentMeasureRegistry::instance().registerType(
        "badge",
        [](loppe::ResolvedNode*, const loppe::ContentMeasureRegistry::Context&,
           float& w, float& h, float) {
            if (w == loppe::AUTO_SENTINEL) w = 24.0f;
            if (h == loppe::AUTO_SENTINEL) h = 20.0f;
        });
}

void Badge::OnSyncFromResolvedNode(const loppe::ResolvedNode& rn) {
    if (!initialized_ && rn.astNode) {         // the initial value is applied once
        auto it = rn.astNode->attrs.find("value");
        if (it != rn.astNode->attrs.end() && it->second.isInt()) value_ = it->second.as<int>();
        initialized_ = true;
    }
    layoutDirty_ = true;
}

void Badge::onRender(loppe::DisplayList& dl, const loppe::ui::Rect& clip) const {
    (void)clip;
    dl.addRect(bounds_, getFillColor(), getStrokeColor(), getStrokeWidth(), getCornerRadius());
    dl.addText(std::to_string(value_), bounds_, getStrokeColor(), 12.0f);
}

bool Badge::handleLocalMouseButton(const loppe::ControlLocalMouseButtonEvent& e) {
    if (!isVisible() || e.action != GLFW_PRESS || !hitTestLocal(e.point)) return false;
    if (loppe::Scene* s = getScene()) {
        if (auto* ui = s->getUIEventSystem()) {
            loppe::UIEventSystem::UIEvent ev;
            ev.type = loppe::UIEventSystem::UIEventType::Click;
            ev.target = this;
            ui->dispatch(ev);
        }
    }
    return true;
}
} // namespace myapp
```

**2) The factory** (`components/my_control_factory.h`): see §3.10, replacing `"dot"` with `"badge"`.

**3) Install** (once, in the host's `Application::build()`):

```cpp
loppe::ControlFactory::setInstance(std::make_unique<myapp::MyControlFactory>());
```

**4) DSL usage**:

```loppe
@style {
    .unread     { background-color:#e5484d; border-radius:10px; }
    .unread.zero{ background-color:#8b8f96; }      /* state expressed as classes */
}
badge(id="unread", class="unread", value=3)
```

**5) Host reaction**:

```cpp
scene->onClick("#unread", [](loppe::UIEventSystem::UIEvent&) { /* open the message panel */ });
```

**6) Wiring**: add `badge.cpp` to `build.lpj`'s `sources`; if `components/` is already an include path, nothing else changes.

---

## 6. Build Wiring (for New Files)

A new control class means new `.cpp/.h` files, and the source list is explicit — nothing is collected automatically:

1. Put the files in a suitable subdirectory of your project (for example `components/`).
2. Add them to `build.lpj`'s `sources` (some projects keep one shared `shared = [ ... ]` list referenced by targets — add them there).
3. Keep one build description: projects use `build.lpj`; do not add a `CMakeLists.txt` (the builder does not read it).
4. When adding a new subdirectory, add its include path too (`include = [ ., components, ... ]`).
5. The engine package already provides the public headers `<components/*.h>`, `<engine/*.h>`, `<render/*.h>` — reference them the way your include list already does.

Build command: in the project directory, `loppe-cli build . Debug` (add `--targets=<name>` to build a single target; names come from your `build.lpj`).
Arguments and output locations: see *Building and Running with loppe-cli* (`LOPPE_CLI_GUIDE_EN.md`).

---

## 7. Common Problems

| Symptom | Cause | Fix |
|---|---|---|
| The tag is in the DSL but the UI is blank | The host never installed a `ControlFactory`, installed it too late, or the hook does not recognise that `type` | Install once at startup; branch on `type` in the hook |
| The type selector `dot { ... }` does not apply | `getTypeName()` returns the C++ class name, or differs in case/spelling from the tag | Return the DSL tag name |
| Internal state changes but nothing repaints | Members changed without `markLayerDirty()` / `invalidate()` | Mark dirty after the change |
| Re-layout every frame, dragging stutters | `onLayoutSynced()` rebuilds unconditionally | Check `layoutDirty_` first |
| Caret blinks, CPU stays high | `invalidate()` inside `update()` every frame | Use `requestAnimationTick()`, and `invalidate()` only on real change |
| One pane scrolls, the other does not follow | The control was not queued into the next frame after the scroll change, so the host never gets the report | Also `invalidate()` or `requestAnimationTick()` |
| Text/graphics spill over neighbouring controls | `onRender` does not `pushClip` to its own viewport | `pushClip` before drawing |
| Long text breaks the card layout | Content size and viewport disagree | See the box model and overflow sections of the layout document |
| Text flickers during held-backspace delete | Dirty layout produced in this frame's `update()` was not caught up | Catch up in `onFrameCommit()` |
| Longer lists switch more slowly | Refreshing by delete-and-rebuild | Reuse elements + update data / toggle classes |
| Styles or events bleed between template instances | A hard-coded `id` in the template, or `Global` scope where `Local` was right | Keep templates id-less; use the `Local` scope |
| Crash when touching controls off-thread | A background thread hit controls directly | `UiDispatcher::post` back to the UI thread |

---

## 8. Pre-Delivery Checklist

Run a self-drawn control (approach 2) through this table before delivery:

| Item | Check |
|---|---|
| Intrinsic size | Fill only dimensions still at `AUTO_SENTINEL`; register one strategy per tag name, not twice |
| Property sync | Apply initial values once via `initialized_`; mark dirty only for properties that really affect size/layout |
| Layout hook | `onLayoutSynced()` checks the dirty flag first; re-layout happens only when the size really changed |
| Scrolling | Compute content size + viewport + scrollbar geometry yourself; clamp offsets after content shrinks |
| Input | Local coordinates + `hitTestLocal`; `captureMouse` / `releaseMouse` for drag/selection; cursor shape per region |
| Time-driven work | Decide inside `update()`, continue only via `requestAnimationTick()`, `invalidate()` only on real change |
| In-frame catch-up | Internal dirty layout from `update()` → fix it up in `onFrameCommit()` to avoid dropped or flickering frames |
| Host events | Dispatch from `update()` after de-duplicating; the host listens via `#id` / `.class` |
| Drawing | `pushClip` to your viewport + `pushTransform(-scrollX, -scrollY)`; background only while layout is not ready |
| Wiring | New files added to `build.lpj` `sources`; new directories added to include paths |

---

## Related Documents

| Document | Contents |
|---|---|
| `LOPPE_DSL_USER_GUIDE_EN.md` | DSL syntax, styling, layout properties (required reading for templates and UIs) |
| `ai/LOPPE_DSL_AI_REFERENCE_EN.md` | Full property tables + key differences from standard CSS (for AI use) |
| `LAYOUT_DESIGN_EN.md` | Size model, box model, scroll containers |
| `ai/LOPPE_CUSTOM_CONTROL_AI_PROMPT_EN.md` | Prompt templates and hard constraints for AI-generated custom controls (for AI use) |
