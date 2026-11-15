# LOPPE From C++ to UI: Your First Application

> 📖 [中文版](LOPPE_GETTING_STARTED.md) | English

---

## 1. What This Document Covers

This document walks from an empty C++ project to an application that displays a `.lui` interface, reacts to clicks, and updates its interface at runtime. It proceeds in four steps: project skeleton → window → load the interface → make the interface respond.

**Audience**: developers who write C++ (can read build scripts and use `std::string`, `std::shared_ptr`, lambdas) and are using LOPPE UI for the first time. No knowledge of engine internals is required.

**Prerequisites**

| Item | Requirement |
|---|---|
| Engine package | A prebuilt LOPPE UI package: `include/` (public headers) plus the library files |
| Compiler | The same toolchain as the engine package (MSVC or clang — see the package notes) |
| Graphics dependencies | GLFW, GLAD, FreeType (shipped with the package or supplied by your project) |
| DSL knowledge | None — this document contains a complete first interface; see the DSL User Guide for syntax details |

**Contents**

| What you want to do | Section |
|---|---|
| Understand how the engine and your code divide the work | §2 |
| Get a window on screen showing the first text | §3 |
| Put the interface in a `.lui` file and load it from disk | §4 |
| Handle clicks, typing, and selection changes | §5 |
| Read and change controls from C++ | §6 |
| Use custom fonts, images, and SVG icons | §7 |
| Build, run, distribute | §8 |
| Something renders or behaves wrongly | §9 |

---

## 2. How the Engine and Your Project Divide the Work

| Who | Responsibility |
|---|---|
| Engine | Parse `.lui`, compute style and layout, draw the control tree into the window, hit-test and dispatch events, measure text and handle scrolling |
| Your project | Create the window and main loop, decide where interface files come from (read the file), hand the parse result to the engine, subscribe to events, change controls from C++, provide fonts / images / icons |

Startup and per-frame order is shown below. You implement two things: an `Application` subclass (the window) and a `Scene` subclass (the interface).

```text
main()
 └ Application::run()
    ├ create window, initialize resources, load a default font, inject the text measurer, register built-in icons (chevron-*)
    ├ build(...)                      ← create your Scene and hand it to Stage
    ├ Scene::setup()                  ← load the .lui, subscribe to events (⚠ no controls exist yet)
    │   └ parse the .lui → hand the result to the document runtime (notifyASTUpdated)
    ├ ← first AST sync: the controls are created here (findControlById returns null before this)
    ├ Scene::onControlsReady()        ← after controls are built (may fire repeatedly; keep it idempotent) → all control edits go here
    └ per-frame loop: style/layout → draw → input events → your callbacks
```

**`setup()` does three things only**: read the interface file, hand the parse result to the document runtime
(`notifyASTUpdated`), and bind events by selector string (`onClick` / `onChange` / `onInput` — binding does not
depend on the control existing yet). Do **not** look controls up and mutate them in `setup()`: they do not exist
there, `findControlById()` silently returns null, and every `setOptions` / `setValue` you wrote is lost. Put
control-mutating initialization in `onControlsReady()` (see §6, "When it is safe to get a control").

---

## 3. How to Build the First Runnable Project

### Step 1: lay out the directory

```text
myapp/
├── build.lpj            build description
├── main.cpp             entry point + Application subclass + Scene subclass
├── ui/
│   └── hello.lui        interface file (written in §4; create the directory now)
└── resources/           resource root: fonts, images, icons
```

### Step 2: write `build.lpj`

An executable is one `exe` target. `sources`, `include`, and `link` are explicit lists — files are never collected automatically, so a new source file must be added to `sources` by hand.

```text
exe myapp {
    sources = [ main.cpp ]
    include = [ ., <engine package>/include ]
    link    = [ loppe, glfw3, glad, freetype ]
    define  = [ LOPPE_STATIC ]
    flags   = [ ${optflags} ]
}
```

| Field | Purpose |
|---|---|
| `sources` | your `.cpp` files (add new files here) |
| `include` | header search directories: your own first, then the engine package's `include` |
| `link` | engine library plus graphics dependencies; on Windows also `opengl32` and `gdi32` |
| `define` | `LOPPE_STATIC` links the engine library statically |

### Step 3: write `main.cpp`

```cpp
#include "application.h"                 // Application
#include "components/stage.h"            // Stage
#include "components/scene.h"            // Scene
#include "engine/document_runtime.h"     // DocumentRuntime

#include <memory>

// Scene: one control tree plus its interaction logic.
class HelloScene : public loppe::Scene {
public:
    explicit HelloScene(DocumentRuntime& runtime) : loppe::Scene(runtime) {}

    bool setup() override {
        // §4 loads the .lui here; §5 subscribes to events here.
        return true;
    }
};

// Application: creates the window and runs the main loop.
class HelloApp : public Application {
public:
    HelloApp() : Application("Hello LOPPE", 800.0f, 600.0f) {}

protected:
    // build() runs once, after the window exists and the engine is initialized.
    bool build(DocumentRuntime& runtime, loppe::Compositor&, float, float) override {
        loppe::Stage::getInstance().replaceScene(std::make_shared<HelloScene>(runtime));
        return true;   // returning false aborts startup and run() exits
    }
};

int main(int argc, char* argv[]) {
    HelloApp app;
    return app.run(argc, argv);   // blocks until the window closes; returns the process exit code
}
```

Points to note:

- `Application(title, width, height)` takes **logical pixels**; the window converts them according to the system scale factor.
- `build()` must return `true` to continue. Create the scene here and hand it to `Stage`.
- `Stage::getInstance().replaceScene(scene)` replaces the current scene (the previous one is destroyed); `pushScene` pushes onto a stack so the previous scene returns when the new one finishes.
- To quit from code (for example a self-test path), call `Application::requestClose()`.

### Step 4: build and run

```bash
loppe-cli build . Debug --targets=myapp
```

The executable lands in `build/Debug/`. Run it and an 800×600 window appears (still empty — §4 adds the interface).

---

## 4. How to Put the Interface in a `.lui` File and Load It From Disk

### The interface file

The division of labour: the interface file describes structure and appearance; C++ owns data and behaviour. Save the following as `ui/hello.lui`:

```loppe
@style {
    .page {
        width: 100%;
        height: 100%;
        background-color: #f5f7fa;
        padding: 24px;
    }
    .title {
        font-size: 24px;
        color: #1f2937;
        margin-bottom: 12px;
    }
    .btn {
        padding: 8px 16px;
        border-radius: 6px;
        background-color: #2563eb;
        color: #ffffff;
    }
}

@layout {
    .page {
        display: flex;
        flex-direction: column;
    }
}

panel(class="page") {
    text:title(class="title", value="Hello, LOPPE"),
    button:btnHello(class="btn") {
        text(value="Click me")
    }
}
```

### Loading in three steps

In `HelloScene::setup()`, do three things: read the file, tell the engine where this `.lui` lives (the base for `~/` paths), and hand the parse result to the document runtime.

```cpp
#include "components/control_manager.h"
#include "components/text.h"
#include "engine/core/parse.h"            // LoppeParser
#include "engine/resource/resource_manager.h"      // ResourceManager

#include <fstream>
#include <sstream>
#include <string>

bool HelloScene::loadDocument(const std::string& path) {
    auto& res = loppe::ResourceManager::getInstance();

    // (1) Locate the file: relative paths resolve against the resource root (and the executable directory)
    const std::string full = res.resolvePath(path);
    std::ifstream in(full, std::ios::binary);
    if (!in) return false;
    std::ostringstream buf;
    buf << in.rdbuf();
    const std::string source = buf.str();

    // (2) Remember this .lui's directory: `~/img/a.png` inside the interface resolves against it
    const auto slash = full.find_last_of("/\\");
    res.setDocumentRoot(slash == std::string::npos ? std::string() : full.substr(0, slash));

    // (3) Parse and hand over to the engine
    loppe::LoppeParser parser;
    auto ast = parser.parse(source).root();
    if (!ast) return false;
    getDocumentRuntime()->notifyASTUpdated(ast);
    return true;
}
```

Then `setup()` becomes:

```cpp
bool setup() override {
    return loadDocument("ui/hello.lui");
}
```

Points to note:

- **Your code owns the parse result** (the `ast` above). The engine never reads the disk and does not know where the file came from — the source can equally come from the network, memory, or an archive.
- `notifyASTUpdated()` runs one styling → layout → control rebuild pass; the interface appears immediately.
- To apply an edited `.lui` without restarting: read the file again, parse again, call `notifyASTUpdated()` again. No need to recreate the window.
- Keep the `ast` in a member so §6 can write runtime values back into the interface description.

---

## 5. How to Make the Interface Respond

`Scene` offers four selector-based convenience bindings; selectors accept `#id` and `.class`:

```cpp
bool setup() override {
    if (!loadDocument("ui/hello.lui")) return false;

    onClick("#btnHello", [this](const loppe::UIEventSystem::UIEvent& e) {
        // e.target        = the control the event currently reached
        // e.deepestTarget = the deepest control actually hit
        setTitle("The button was clicked");
    });

    onInput("#nameInput", [](const loppe::UIEventSystem::UIEvent&) { /* text changed */ });
    onChange("#cityBox",   [](const loppe::UIEventSystem::UIEvent& e) { /* e.text = selected item */ });
    onScroll("#list",      [](const loppe::UIEventSystem::UIEvent&) { /* wheel */ });
    return true;
}
```

For finer-grained events, bind through the event system and name the event type:

```cpp
getUIEventSystem()->bind(
    "#btnHello",
    loppe::UIEventSystem::UIEventType::MouseDown,
    [](const loppe::UIEventSystem::UIEvent&) { /* pressed */ });
```

**Event types**

| Type | When it fires | Dispatched by |
|---|---|---|
| `Click` | press and release on the same control | any hit-testable control |
| `MouseDown` / `MouseUp` | mouse button pressed / released | any hit-testable control |
| `Input` | a text control's value changed | `input` |
| `Change` | the control's own options or selection changed | `combobox` |
| `Scroll` | mouse wheel (the pointer-event form of `onScroll`) | scroll containers |
| `Focus` / `Blur` | input focus gained / lost | `input` |
| `KeyDown` / `KeyUp` | key pressed / released | the focused control |

**How an event reaches your handler**

1. The engine hit-tests the pointer and builds a path from the root to the deepest node.
2. The event goes to the deepest node first, then bubbles up the ancestor chain, so a binding on a parent container also receives events from its children.
3. Setting `e.stopPropagation = true` in a handler stops the bubble; handlers registered after it on the same node are skipped as well.

Events are dispatched on the UI thread. After a background thread produces data, hand it to the engine across threads and touch controls from a UI-thread callback.

---

## 6. How to Read and Change the Interface From C++

### Getting a control

Look a control up by `id`, then cast it to its concrete type:

```cpp
void setTitle(const std::string& text) {
    auto* manager = getControlManager();
    if (!manager) return;
    auto control = manager->findControlById("title");
    if (auto label = std::dynamic_pointer_cast<loppe::Text>(control)) {
        label->setText(text);
    }
}
```

Common runtime interfaces:

| Control | Runtime interface |
|---|---|
| `text` | `setText` / `getText` / `setFontSize` / `setTextColor` / `setAlignment` / `setWrap` |
| `input` | `setValue` / `getValue` / `setPlaceholder` / `setType` / `selectAll` / `selection` |
| `progress` | `setProgress(0..1)` / `setBarColor` |
| `combobox` | `setOptions` (replaces the list) / `setSelectedIndex` / `getSelectedIndex` / `setPlaceholder` / `setArrowIcon` (icon name of the expander arrow) |
| `radio` | `setChecked` / `isChecked` / `setGroup` |
| `icon` | `setName` |
| `image` | `setSrc` |
| any control | `setEnabled(false)` enters the disabled state; a disabled control receives no hover, press, or focus |

Control interfaces may only be called on the UI thread, as with all controls.

### When it is safe to get a control

Controls are **not** created by `setup()`: `notifyASTUpdated()` only hands the AST to the document runtime, and the actual control tree appears on the **first frame's AST sync** (`sync(structural)` inside `ViewCoordinator::applyASTRepaint`) — long after `setup()` returned. So inside `setup()` `findControlById()` **always returns null**, silently doing nothing (a combobox with no options, an input with no initial value, a progress bar that never moves usually come from here).

Put every control-mutating initialization in `Scene::onControlsReady()`: it fires after the controls exist and before the frame renders; it may run several times per frame and a tree rebuild replaces control instances, so try on each call and keep each step idempotent.

```cpp
void onControlsReady() override {
    // Idempotent: may fire several times per frame, and a re-parse replaces the controls
    if (auto control = getControlManager()->findControlById("cityBox"))
        if (auto box = std::dynamic_pointer_cast<loppe::ComboBox>(control))
            if (box->getSelectedIndex() < 0) box->setSelectedIndex(0);
}
```

After the interface is **re**-parsed (for example when switching pages) the old control pointers are invalid — re-fetch them in this callback too.

### Writing runtime values back into the interface description

Content typed into an `input` is runtime state; the `value` in the interface description is not updated automatically. To persist what the user entered:

```cpp
syncToAST(astRoot_, /*overwrite=*/true);   // astRoot_ = the root from §4 (a Scene member method)
```

With `overwrite=false` (the default), only nodes that have no `value` in the description are written.

---

## 7. How to Add Fonts, Images, and Icons

### Resource root

Relative paths (and image `src` values) are resolved in this order:

1. A `~/` prefix: **relative to the directory of the current `.lui` file** (requires `setDocumentRoot`, see §4).
2. An absolute path, if the file exists.
3. The resource root set by `setResourceRoot(path)`, including its `resources/` subdirectory.
4. The executable's directory, including its `resources/` subdirectory.

Set the resource root in `build()`:

```cpp
bool build(DocumentRuntime& runtime, loppe::Compositor&, float, float) override {
    loppe::ResourceManager::getInstance().setResourceRoot("resources");
    loppe::Stage::getInstance().replaceScene(std::make_shared<HelloScene>(runtime));
    return true;
}
```

### Fonts

If no font has been registered when the engine starts, it tries to load a system default font (available on desktop platforms, fails safely on embedded ones), so a minimal project needs no font setup. To pick a specific font:

```cpp
auto& res = loppe::ResourceManager::getInstance();
res.loadFont("resources/fonts/NotoSansSC-Regular.ttf", "body");  // registered name
res.setDefaultFont("body", 14.0f);                               // default family and size
```

Use it in the interface by family name: `font-family: body;`. **CJK fonts usually carry no emoji glyphs** — if you need emoji, register a second font that does and set `font-family` on the nodes that need it.

### Images

```loppe
image(class="logo", src="~/img/logo.png")     /* relative to this .lui */
image(class="photo", src="pics/a.jpg")        /* relative to the resource root */
```

Images load asynchronously; the engine schedules a repaint when loading finishes, so the host does not poll. With no `width` / `height`, the image's own pixel size feeds measurement; supply them and that size is used instead. `background-image` follows the same path rules but only accepts local file paths, not network URLs.

### Icons

An icon is SVG text registered with the engine once at startup, then referenced by name in the interface:

```cpp
loppe::svg::RegisterIcon("close", "<svg viewBox=\"0 0 16 16\">…</svg>");
```

```loppe
icon(class="icon", name="close")
```

Icons are tinted by the current `color` and sized by `width` / `height`. Because the tint is a style property, `icon:hover { color: … }` changes the colour on hover.

---

## 8. How to Build, Run, and Distribute

```bash
# build (current directory project, Debug configuration, target name from build.lpj)
loppe-cli build . Debug --targets=myapp

# validate build.lpj and .lui syntax only, produce nothing
loppe-cli config .
```

Commands, arguments, output locations, and how to read build diagnostics: see *Building and Running with loppe-cli* (`LOPPE_CLI_GUIDE_EN.md`). Projects are described by `build.lpj` only — there is no `CMakeLists.txt` or `.sln`.

| Item | Notes |
|---|---|
| Output | the executable under `build/<configuration>/` |
| Needed at runtime | the executable, `resources/` (fonts, images), the interface `.lui` files |
| Engine library | with `LOPPE_STATIC` it is linked into the executable; nothing extra to ship |
| Interface file location | decided by your loading code (§4). Placing them next to the executable or under its `resources/` needs no extra resource-root configuration |

---

## 9. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| Window opens but stays blank | `build()` created no scene, or `setup()` never handed the parse result to the runtime | Confirm `replaceScene` is called; confirm `setup()` returns `true` and calls `notifyASTUpdated()` |
| Interface file not found | the path is neither under the resource root nor next to the executable | Print what `resolvePath(path)` returns, or call `setResourceRoot` explicitly |
| `~/` images fail to load | no document root was set | Call `setDocumentRoot(directory of the .lui)` while loading it |
| Text shows as boxes, or CJK does not render | the current font lacks those glyphs | `loadFont` a font with the needed coverage and `setDefaultFont` |
| Clicks do nothing | the bound selector does not match the `id` / `class` in the interface; or the control is disabled | Check the `#id`; confirm state with `isEnabled()`; bindings on parents rely on bubbling — check for a `stopPropagation` in between |
| Input content cannot be read back | runtime values were never written to the interface description | Call `syncToAST(astRoot, true)` before saving |
| Editing the `.lui` has no effect | the file is loaded once at startup | Read, parse, and call `notifyASTUpdated()` again |
| `findControlById` returns null | controls do not exist yet during `setup()` (they appear after the first AST sync), or the interface was re-parsed since | Look controls up in `onControlsReady()` (idempotent), never in `setup()`; re-acquire there after interface updates |
| A dropdown opens but looks like nothing happened | the option list is empty → popup height = item count × 30 = 0 | Feed data with `setOptions()` (`options` only covers static lists); the code doing so is probably wrongly placed in `setup()` |
| A dropdown has no arrow on the right | `arrow` names an icon that neither the engine ships nor the host registered | `chevron-down` (default) plus `chevron-up` / `chevron-right` / `chevron-left` are built in; any other name must be registered with `svg::RegisterIcon` first |

---

## 10. Next Steps

| Topic | Document |
|---|---|
| `.lui` syntax, selectors, style and layout properties | DSL User Guide |
| Size model, flex / grid space allocation | Layout Design |
| Attributes and interfaces of every built-in control | Built-in Controls Reference |
| Implementing controls the engine does not ship | Custom Control User Guide |

---

## Related Documents

| Document | Contents |
|---|---|
| `LOPPE_DSL_USER_GUIDE_EN.md` | DSL syntax, selectors, all style and layout properties, full examples |
| `LOPPE_BUILTIN_CONTROLS_EN.md` | Built-in control attributes, default styling, events, runtime interfaces |
| `LAYOUT_DESIGN_EN.md` | Deterministic layout sizing model and space allocation rules |
| `LOPPE_CUSTOM_CONTROL_GUIDE_EN.md` | Compose existing controls / self-drawn control types / appearance only |
| `LOPPE_MULTI_UI_EN.md` | More than one `.lui`: shell plus mount points, switching screens, shared styles |
| `ai/LOPPE_DSL_AI_REFERENCE_EN.md` | Full property tables and differences from standard CSS (for AI use) |
