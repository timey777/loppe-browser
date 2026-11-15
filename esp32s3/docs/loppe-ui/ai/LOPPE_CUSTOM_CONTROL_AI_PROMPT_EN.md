# LOPPE Custom Control AI Prompts (AI Edition)

> Compact version for AI context injection: hard constraints, approach decision, API contracts, skeletons, paste-ready prompt templates.
> Companion long-form doc: [Custom Control User Guide](../LOPPE_CUSTOM_CONTROL_GUIDE_EN.md).

> 📖 [中文版](LOPPE_CUSTOM_CONTROL_AI_PROMPT.md) | English

---

## 1. Hard Constraints (violating any of these is wrong)

1. **Write code only in your own project**: the engine ships as a prebuilt package (`lib/*.a` + `include/*.h`)
   with public headers — use only those APIs and do not invent engine-internal interfaces.
2. **Use only the three extension points**: composite mounting (`ComponentInstantiator`), new control type
   (`ControlFactory` subclass), appearance/state (DSL `@style` + classes). Do not invent a fourth.
3. **A new tag needs both halves**: an `AbstractControl` subclass (`getTypeName()` returns the **DSL tag name**)
   *and* a `ControlFactory` subclass (the hook recognises that `type`) — otherwise the tag produces no control.
4. **Install the factory once at startup**: `ControlFactory::setInstance(...)`, before documents are parsed / controls are created.
5. **Only fill `AUTO_SENTINEL`**: intrinsic measurement must not override sizes explicitly written in style.
6. **No hard-coded ids**: reusable DSL templates must be id-less (instantiating twice collides).
7. **UI thread only**: background threads must `UiDispatcher::post(...)` before touching controls.
8. **Draw only in the render phase**: `onRender` must not mark dirty or rebuild layout.
9. **Wire up new files**: add them to `build.lpj`'s `sources` and add `include` paths for new directories.
   (A project's only build description is `build.lpj` — there is no `CMakeLists.txt`; build with `loppe-cli build . Debug`,
   see `../LOPPE_CLI_GUIDE_EN.md`.)
10. **Do not re-implement engine capabilities**: background/border/radius/padding/visibility/scrollbars/text editing/
    double-and-triple-click word selection all come from the engine (base-class mapping + shared components).

---

## 2. Approach Decision (ask three questions)

```
(1) Can it be assembled from existing controls (panel/text/icon/image/button/input/…)?
       Yes → Approach 1, compose existing controls (DSL template + SpawnElementViewUnderId); mount code only, no control class
       No  ↓
(2) Is it only a different look/state while structure and behaviour suffice?
       Yes → Approach 3, appearance only: @style + classes + :hover/:focus/:disabled; no C++
       No  ↓
(3) Must draw pixels / compute its own layout / take over keyboard and scrolling → Approach 2, self-drawn control type
       = AbstractControl subclass (+ intrinsic-size registration) + ControlFactory subclass + startup setInstance
```

Approaches 1 and 2 combine: a self-drawn control can still build its skeleton from DSL snippets.

---

## 3. API Contracts

### 3.1 Must implement (pure virtual)

```cpp
std::string getTypeName() const override;                  // case-insensitively == the DSL tag
void onRender(DisplayList& dl, const ui::Rect& clip) const override;   // draw only
```

### 3.2 Overridable hooks (as needed — do not copy them all)

| Signature | Timing / purpose |
|---|---|
| `void OnSyncFromResolvedNode(const ResolvedNode& rn)` | Sync derived properties after the generic mapping; ⛔ apply initial values once (`initialized_` guard) |
| `void onLayoutSynced()` | After bounds/viewport/scroll sync → rebuild your layout cache; **must** check the dirty flag first |
| `void onFrameCommit()` | After update, before paint → catch up internal dirty layout produced this frame |
| `void update()` | Time-driven work; to continue → `requestAnimationTick()` |
| `bool handleLocalMouseButton(const ControlLocalMouseButtonEvent&)` | Local coordinates + `hitTestLocal`; drag with `captureMouse()` / `releaseMouse()` |
| `bool handleLocalMouseMove(const ControlLocalMouseMoveEvent&)` | Hover / drag-selection |
| `bool handleLocalScroll(const ControlLocalScrollEvent&)` | Wheel |
| `bool onKey(const KeyEvent&)` / `bool onChar(const CharEvent&)` | While focused; `true` = handled |
| `void onFocus()` / `void onBlur()` / `void onHover(bool)` / `void onActive(bool)` | State callbacks |
| `void setScrollSize(float, float)` | Self-managed scrollers override it as a no-op (compute your own) |
| `bool isContainer() const` | Return `true` to have children built as child controls |

### 3.3 Available tools (provided by the base class)

```cpp
// Appearance (already synced by the generic mapping — just consume it)
getFillColor() / getStrokeColor() / getStrokeWidth() / getCornerRadius() / getPadding*() / isVisible() / bounds_
// State
hasClass(c) / addClass(c, forceLayout=false) / removeClass(c, forceLayout=false) / setId / getUid
// Scrolling
setScroll / getScrollX|Y / setScrollSize / getScrollWidth|Height / setViewportSize / getViewportWidth|Height / hasViewport
// Requesting work
markLayerDirty()            // dirty this control only
invalidate()                // dirty + repaint request + tick next frame
requestAnimationTick()      // keep ticking, no repaint
captureMouse() / releaseMouse() / hitTestLocal(point) / toLocalPoint(point)
getScene()->setCursor(Scene::Cursor_Hand|Arrow|IBeam)
```

### 3.4 Other entry points

```cpp
// Intrinsic size (fill only dimensions that are AUTO)
loppe::ContentMeasureRegistry::instance().registerType("mytag", strategy);
//   strategy: void(ResolvedNode*, const Context& ctx, float& w, float& h, float availableWidth)
//   ctx.textMeasurer measures text; if (w == AUTO_SENTINEL) w = ...;

// Composite mounting (the return value IS the success signal)
ElementView              ComponentInstantiator::SpawnElementViewUnderId (Scene*, parentId, dsl|AstSnippet, ctx, index=-1, ResolvedRuleScope=Local);
std::vector<ElementView> ComponentInstantiator::SpawnElementViewsUnderId(Scene*, parentId, dsl|AstSnippet, ctx, index=-1, ResolvedRuleScope=Local);
// ElementView: valid()/control()/id()/uid()/addClass/removeClass/hasClass/remove()/ancestorByClass/descendantByClass

// Component lifecycle shell
class X : public UIComponent { void onAttach(Scene*, const std::string& parentId) override; };

// New-tag registration
class MyFactory : public ControlFactory {
    std::shared_ptr<AbstractControl> doMakeControlFromResolvedNodeHook(ResolvedNode* rn) override;   // return nullptr if not yours
    std::shared_ptr<ScrollbarControl> CreateScrollbar(Orientation) override;                        // optional
};
ControlFactory::setInstance(std::make_unique<MyFactory>());   // once, at startup

// Control → host events
UIEventSystem::UIEvent e; e.type = UIEventType::Change; e.target = this; e.text = ...;
getScene()->getUIEventSystem()->dispatch(e);
// Host listens: scene->onClick("#id"|".class", cb) / onInput / onChange / onScroll
```

---

## 4. Four Disciplines (check after writing)

| Discipline | Rule |
|---|---|
| **Size** | Explicit sizes are owned by style; `auto` goes through `ContentMeasureRegistry`; do not resize every frame |
| **Scrolling** | Self-managed: no-op `setScrollSize` + `AbstractControl::setScrollSize(yours)` + `setViewportSize(content area)` + clamp offsets |
| **Repaint** | Appearance changed → `markLayerDirty()`; to notify the host / keep animating → `invalidate()` or `requestAnimationTick()`; **never** `invalidate()` every frame |
| **Events** | Dispatch from `update()` **after de-duplicating**; handlers run deepest-first along the hit path; `stopPropagation` halts propagation |

---

## 5. Code Skeletons

### 5.1 Composite (no control class)

```cpp
static const loppe::AstSnippet kTpl = []{ loppe::LoppeParser p; return p.parse(R"ui(
@style  { .card { background-color:#1f2733; border-radius:8px; padding:12px; } }
@layout { .card { display:flex; flex-direction:column; } }
panel(class="card") { text(class="t", value="Title"), text(class="v", value="0") }
)ui"); }();

loppe::ElementView v = loppe::ComponentInstantiator::SpawnElementViewUnderId(scene, parentId, kTpl, scene->getDocumentRuntime());
if (!v) return;
```

### 5.2 Self-drawn control

```cpp
class MyControl : public loppe::AbstractControl {
public:
    MyControl() { ensureMeasureRegistered(); }
    std::string getTypeName() const override { return "mytag"; }
    static void ensureMeasureRegistered() {
        static bool once = false; if (once) return; once = true;
        loppe::ContentMeasureRegistry::instance().registerType("mytag",
            [](loppe::ResolvedNode*, const loppe::ContentMeasureRegistry::Context&, float& w, float& h, float) {
                if (w == loppe::AUTO_SENTINEL) w = 120.0f;
                if (h == loppe::AUTO_SENTINEL) h = 32.0f;
            });
    }
protected:
    void onRender(loppe::DisplayList& dl, const loppe::ui::Rect& clip) const override;
    void OnSyncFromResolvedNode(const loppe::ResolvedNode& rn) override;
    void onLayoutSynced() override { if (!layoutDirty_) return; rebuild(); }
    bool handleLocalMouseButton(const loppe::ControlLocalMouseButtonEvent& e) override;
private:
    bool  initialized_ = false;
    bool  layoutDirty_ = true;
    void  rebuild()    { /* your internal layout cache */ layoutDirty_ = false; }
};
```

### 5.3 Factory + install

```cpp
class MyFactory : public loppe::ControlFactory {
public:
    std::shared_ptr<loppe::AbstractControl> doMakeControlFromResolvedNodeHook(loppe::ResolvedNode* rn) override {
        if (rn && rn->astNode && rn->astNode->type == "mytag") return std::make_shared<MyControl>();
        return nullptr;   // fall back to built-in types
    }
};
// Inside the host's Application::build():
loppe::ControlFactory::setInstance(std::make_unique<MyFactory>());
```

---

## 6. Prompt Templates (copy-paste ready)

### Template 1: composite component

```
I am building a host application on LOPPE UI (loppe-ui ships as a prebuilt package with public headers).
Implement a "<one-line description, e.g. statistics card with title and value>" component using the composite approach:
- one DSL template (with its own local @style/@layout rules), parsed once with AstSnippet::parse, written id-less;
- mount it under the given parentId with ComponentInstantiator::SpawnElementViewUnderId, returning an ElementView;
- provide setData(...) to refresh the display, preferring "reuse elements + toggle classes / update text" over delete-and-rebuild;
- express state as classes (e.g. .active) and interaction with :hover/:focus/:disabled;
- output only the .h/.cpp files plus the list of filenames to add to build.lpj sources.
Use only APIs that exist in the public headers; do not invent interfaces.
```

### Template 2: self-drawn control type

```
I am building a host application on LOPPE UI (loppe-ui ships as a prebuilt package with public headers).
Implement a new DSL control type `<tag>` (C++ class <Name>) that <feature description>.
You must follow these rules:
1) derive from loppe::AbstractControl and implement onRender(DisplayList&, const ui::Rect& clip) and
   getTypeName() (returning "<tag>", case-insensitively equal to the DSL tag);
2) register intrinsic size in the constructor via ContentMeasureRegistry::registerType("<tag>", strategy),
   filling only dimensions that == AUTO_SENTINEL;
3) sync derived properties in OnSyncFromResolvedNode(rn) (DSL attributes via rn.astNode->attrs;
   style via rn.getFinalStyle()/rn.props()), applying initial values exactly once (initialized_ guard);
4) consume generic appearance (background/border/radius/padding/visibility) from base-class getters — never re-parse it;
5) rebuild internal layout in onLayoutSynced() but check the dirty flag first (unconditional rebuild is forbidden);
6) state changes use markLayerDirty(); to notify the host, dispatch UIEventSystem events from update()
   after de-duplicating (the host listens via scene->onClick/onInput/onChange);
7) also provide the ControlFactory subclass (doMakeControlFromResolvedNodeHook recognising "<tag>", returning nullptr otherwise)
   plus the startup ControlFactory::setInstance call site;
8) the DSL usage is: <tag>(id="x", ...).
Output: control .h/.cpp, factory .h, integration snippet, and the build.lpj sources delta.
```

### Template 3: appearance-only variant

```
This is an existing LOPPE UI control/component: <control name or .lui snippet>.
Without writing C++, reshape it into <target look/state> using DSL and classes only:
- allowed: edits to @style/@layout rules, adding/removing/combining classes, :hover/:active/:focus/:disabled pseudo states,
  icons already present in the SVG registry;
- not allowed: adding C++ control types, using properties/selectors that do not exist, hard-coded inline sizes (prefer flex/percentages);
- output the complete replacement .lui snippet plus the list of classes the host must toggle;
- mind engine differences: overflow defaults to auto (not visible), always border-box, no flex-basis,
  no descendant/child combinators in selectors.
```

---

## 7. Anti-Patterns (what AI typically gets wrong)

| ❌ Wrong | ✅ Right |
|---|---|
| Adding a type branch inside the engine implementation | Your own `ControlFactory` subclass + `setInstance` |
| `getTypeName()` returns the C++ class name | Return the DSL tag (lowercase) |
| Custom tag with no factory | Provide the factory, or compose existing tags in DSL |
| Factory installed late (control tree already built) | Install during startup in `Application::build()` |
| Intrinsic measurement overwrites `requestedW/H` unconditionally | Fill only when `== AUTO_SENTINEL` |
| Unconditional rebuild inside `onLayoutSynced()` | Check `layoutDirty_` first |
| `invalidate()` every frame to drive animation | `requestAnimationTick()`, `invalidate()` only on real change |
| Only `markLayerDirty()`, expecting the host to receive scroll reports | Queue the control into the next frame (`invalidate()`) |
| Marking dirty / rebuilding layout inside `onRender` | Do it in event handlers or `update()` / `onFrameCommit()` |
| Hard-coded `id` in a template mounted several times | Id-less template, or inject a unique id |
| Refreshing lists with delete-and-rebuild | Reuse elements + update data / toggle classes |
| Self-drawn content overflowing its own box | `pushClip` to your viewport (engine text leaves do not clip themselves) |
| Background thread touching controls | `UiDispatcher::post` back to the UI thread |
| Forgetting `build.lpj sources` for new files | Update sources and include paths |

---

## 8. Delivery Checklist

- [ ] Only public-header APIs used; all new code in your own project
- [ ] `getTypeName()` == DSL tag; the factory hook recognises that type; `setInstance` runs at startup
- [ ] Intrinsic measurement fills only `AUTO_SENTINEL`
- [ ] `OnSyncFromResolvedNode` guards initial values with `initialized_`; derived changes mark dirty
- [ ] `onLayoutSynced()` checks the dirty flag; no heavy work in the render phase
- [ ] Repaint via `markLayerDirty()`/`invalidate()`; time-driven work via `requestAnimationTick()`
- [ ] Events dispatched from `update()` after de-duplicating; host listens via `scene->on*("#id"|".class", cb)`
- [ ] Self-drawn content `pushClip`-ed to its viewport; `onRender` marks nothing dirty
- [ ] New files added to `build.lpj sources`; new directories added to include paths
- [ ] The new tag works in `.lui`, verified by at least a headless test or a minimal UI

---

## 9. Quick Reference

**`UIEventType`**: `Click` `MouseDown` `MouseUp` `Input` `Change` `Scroll` `Focus` `Blur` `KeyDown` `KeyUp`

**Selectors** (`bind` / `scene->onXxx`): `#id`, `.class`; style selectors additionally support `type` and
`:hover/:active/:focus/:disabled` — descendant/child combinators are **not** supported.

**Built-in tags**: `panel` `text` `button` `input` `combobox` `radio` `progress` `icon` `image` `scrollbar` (managed automatically)

**Related documents**

| Document | Purpose |
|---|---|
| `../LOPPE_CUSTOM_CONTROL_GUIDE_EN.md` | Full custom-control guide (complete example + pitfall table) |
| `../LOPPE_DSL_USER_GUIDE_EN.md` | Writing UI in DSL |
| `LOPPE_DSL_AI_REFERENCE_EN.md` | Full property tables + key differences from standard CSS (same folder) |
| `../LAYOUT_DESIGN_EN.md` | Size model / box model / scroll containers |
