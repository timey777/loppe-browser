# Multiple `.lui` Files in One Project

> 📖 [中文](LOPPE_MULTI_UI.md) | English

This document covers one point: **a project is not limited to a single `.lui` file, and usually should not be.**

Intended readers: people who already have a working single-`.lui` project (desktop / Android / ESP32-S3) and want to split the UI up, plus AI assistants generating code from these docs. Prerequisites: you know a `.lui` file consists of `@style` / `@layout` / a node tree (see the DSL User Guide), and you know how to hand one `.lui` file to the engine (see Getting Started §4; on embedded see `embedded/README.md` and `EmbeddedHost`).

Contents: §1 the three layouts and how to choose; §2 "shell + several page files"; §3 swapping the whole UI; §4 sharing styles between files; §5 platform differences; §6 common mistakes; §7 checklist.

---

## 1. Three Ways to Organise

The node tree in one `.lui` file is one control tree. To hold more UI in a project there are exactly three legal approaches:

| Approach | Structure | When | Cost |
|---|---|---|---|
| A. Sections in one file | One `.lui`, split with `panel`, switch with `class` / `display` | Very small UI | Gets unwieldy fast; every page shares one `@style` namespace |
| B. Shell + page files | One shell `.lui` with a mount point (`id="…"`), one `.lui` per page, mounted at runtime | **Default recommendation**: multiple pages, panels, reusable blocks | Needs a little C++ (read file + mount) |
| C. Swap the whole file | One complete `.lui` per page; hand the whole file to the engine on switch | Pages share no shell (full-screen wizards, standalone tools) | Whole tree rebuilds on each switch |

**The engine has no "import another `.lui`" syntax.** `.lui` files never include each other; concatenation happens on the C++ side — read several texts, hand each to the engine. §2 / §3 are the two ways to do that.

> **Note for AI assistants**: do not assume a project has just `main.lui`. First list the UI directory (desktop `ui/`, Android `app/src/main/assets/ui/`, ESP32 project `main/`), then check the C++ for `SpawnElementViewUnderId` / repeated `loadUi`. In a project that is already split, a new page means **a new `.lui` file plus a mount**, not more content inside `main.lui`.

---

## 2. Shell + Several Page Files

This is how the IDE itself is organised, and it is the cheapest shape for a multi-page project: one **shell** holds only the shared chrome (top bar, sidebar, status bar) and leaves an anchor for the part that changes; each page is its own `.lui`, mounted under that anchor at runtime.

### 2.1 Leave a mount point in the shell

```loppe
@style {
    .app    { width: 100%; height: 100%; background-color: #0f141c; padding: 12px; }
    .header { height: 44px; background-color: #1b2431; border-radius: 6px; }
    .slot   { width: 100%; flex-grow: 1; }        /* the changing part: fills remaining height */
}
@layout {
    .app { display: flex; flex-direction: column; }
}

panel(class="app") {
    panel(class="header") { text(value="Top bar, shared by every page") },
    panel(id="page-slot", class="slot")           /* ← mount point: empty for now */
}
```

A mount point is an **empty `panel` with an `id`**. The `id` is what lets the runtime find it; `flex-grow: 1` gives the mounted page the remaining space. Remember that container `overflow` defaults to `auto`, so the height chain has to be written all the way down (see `LAYOUT_DESIGN.md`).

### 2.2 One `.lui` per page

`ui/home.lui`:

```loppe
@style {
    .home { width: 100%; background-color: #16202c; border-radius: 8px; padding: 12px; }
    .title { font-size: 16px; color: #e6f1ff; }
    .btn  { height: 40px; background-color: #2563eb; border-radius: 6px; margin-top: 10px; }
    .btn:active { background-color: #4ecca3; }
}
@layout {
    .home { display: flex; flex-direction: column; }
    .btn  { display: flex; align-items: center; justify-content: center; }
}

panel(class="home") {
    text(class="title", value="Home"),
    button(id="btn-go-settings", class="btn") { text(value="Settings") }
}
```

`ui/settings.lui` follows the same shape. Points to note:

- Each page file **may have its own `@style` / `@layout`** — they only affect the nodes mounted by that call (default `ResolvedRuleScope::Local`, see §4).
- Page files **should not repeat what the shell already provides** (background, full-page size, top bar). Their root node describes only "this region".
- When a page uses `~/` paths (background images), the base is **the directory last given to the engine** (on desktop `ResourceManager::setDocumentRoot`). If page files sit next to the shell you need not care; for subdirectories see §5.

### 2.3 Mount onto the mount point

```cpp
#include "components/component_instantiator.h"   // ComponentInstantiator
#include "components/element_view.h"             // ElementView (for unmounting)
#include "components/scene.h"

// Read one .lui as text (path depends on your platform; on desktop std::ifstream is fine)
std::string readText(const std::string& path);

// The currently mounted page. The ElementView returned by the spawn call is your
// unmount handle — keep it, see §2.4.
loppe::ElementView g_page;

void showPage(loppe::Scene* scene, const std::string& luiPath)
{
    g_page.remove();                                  // detach the previous page (no-op on an empty handle)
    g_page = loppe::ComponentInstantiator::SpawnElementViewUnderId(
        scene, "page-slot", readText(luiPath), scene->getDocumentRuntime());
    if (!g_page) {
        // mount failed: parent node missing, or this DSL failed to parse
    }
}
```

`SpawnElementViewUnderId` instantiates a DSL snippet as a template; its top-level content nodes become children of `page-slot`. Test the returned `ElementView` with `if (...)` for success. Switching pages is two statements: detach the old one, mount the new one.

### 2.4 Unmounting and rebuilding

- **Unmount with `ElementView::remove()`**: store the value returned by `SpawnElementViewUnderId` and call it before switching. The structure is detached on the same frame (layout and hit-testing stop seeing it right away); the engine frees memory at end of frame, so never `delete` it yourself.
- One handle for "the current page" is enough: `remove()` detaches the whole mounted subtree, so child controls need no bookkeeping.
- Mounting the same page twice puts two copies on screen. **remove first, then mount.**
- `id` values in page files must be unique **across the whole document** (see §6).

For bulk management (list rows, reusable card collections) the engine also offers `ComponentHost`: `Mount()` mounts a template once, `SetRows()` reconciles rows by stable key, `Unmount()` removes everything in one call — less plumbing than hand-written spawn/remove (see `components/component_host.h`; comparison in §2.6).

### 2.5 Bind events on the C++ side

Mounted controls accept selector-based events just like any other:

```cpp
scene->onClick("#btn-go-settings", [scene](loppe::UIEventSystem::UIEvent&) {
    showPage(scene, "ui/settings.lui");
});
```

Selectors support `type` / `.class` / `#id` and same-element combinations only — **no descendant selectors** — so `id` / `class` names must not collide between page files, or one rule will hit several places.

### 2.6 List rows: never write `@placeholder@` in a `.lui`

"Lay out the row now, fill in the data later" invites a marker in the `.lui` plus a string replacement in C++:

```loppe
panel(class="node-grid") {
    @ROWS@                 // ❌ parse error
}
```

**`.lui` has no template / placeholder / loop syntax.** `@` only appears in the three block headers
`@style` / `@layout` / `@version`; `@ROWS@` is treated as an unknown block header and parsing fails.
The file also fails in the editor the moment it is opened, and a runtime `parse` fails the same way.

Doing the substitution before `parse` so it "happens to work" is still wrong: the file itself is invalid
(no editor, no preview, no editing), and text substitution corrupts the DSL as soon as a value contains
a `"` or a newline.

Pick one of three approaches by list size — none of them needs a placeholder:

| Size | Approach | Prior art |
|---|---|---|
| Tens of rows, fixed structure | Leave an empty container `panel(id="rows-host") {}`, build the row DSL in C++ and mount it; `remove()` the old one before re-mounting | Editor welcome page "recent files" (`refreshWelcomeList` in `editor/panels/welcome_panel.cpp`) |
| Batches of rows, add/remove by key | `ComponentHost`: `Mount()` the template once, `SetRows()` reconciles by stable key | `components/component_host.h` |
| Hundreds to thousands of rows, trees | Self-drawn control (paint rows yourself; no per-row controls) | Editor file tree, `editor/components/tree_view.cpp` |

The first one is the common case:

```loppe
// ui/pages/proxies.lui — the container stays empty and is valid DSL
panel(class="node-grid", id="rows-host") {}
```

```cpp
// build rows once the data arrives, mount them under the container
std::string rows;
for (size_t i = 0; i < nodes.size(); ++i) {
    if (i) rows += ",";
    rows += "panel(class=\"node-card\", id=\"n" + std::to_string(i) + "\") {"
            " text(class=\"node-name\", value=\"" + Esc(nodes[i].name) + "\") }";
}
rowsView_.remove();                       // detach the old rows first, or you get two copies
rowsView_ = ComponentInstantiator::SpawnElementViewUnderId(this, "rows-host", rows,
                                                           getDocumentRuntime());
```

Rules that bite:

- **Escape values**: prefix `"` and `\` with a backslash (`Esc()` does exactly that). The engine exports no
  such helper — write the ten-line function in the host.
- **Row DSL carries no `@style`**: each row would register its own CSS and that accumulates. Put styles in
  the page file's `@style` and have rows only reference classes; with `ComponentHost::SetRows`, the classes
  come from the template passed to `Mount()`.
- **Bind row events once via class delegation** (`scene->onClick(".node-card", ...)`), never one closure per
  row — old closures stay in the event system after rows are rebuilt.

---

## 3. Swapping the Whole UI

When pages share no shell (full-screen wizards, standalone tool pages), write each `.lui` as a complete UI and hand the whole file to the engine on switch:

```cpp
// Desktop: parse again + notifyASTUpdated (this is the "reload a .lui" path from Getting Started §4)
bool loadDocument(const std::string& path);   // the three steps from Getting Started §4

// Embedded: EmbeddedHost accepts another file directly
host.loadUi(readText("main/boot.lui"));
```

- Desktop / Android: `LoppeParser::parse` → `notifyASTUpdated(ast)`. The engine diffs by node identity, so unchanged parts are not rebuilt wholesale.
- Embedded: `EmbeddedHost::loadUi(...)` can be called repeatedly; if parsing fails the screen keeps its current content (no blank frame).
- Pages switched this way **must carry complete styling** (background, full-page size) since no shell backs them.

---

## 3.1 Things to Watch When Switching

- **A switch rebuilds the tree**: control handles obtained via `findControlById` become invalid and must be re-fetched.
- **Events bound once do not need rebinding**: `onClick("#id", …)` binds to the scene by selector, not to a control pointer; after the rebuild the new control matches the same selector. The precondition is a stable, unique `id`.
- **The first frame repaints fully**: on embedded, structural change produces large dirty regions and `EmbeddedHost` degrades to a full flush by itself.

---

## 4. Sharing Styles Between Files

`.lui` files do not import each other, so a "shared style sheet" is expressed through **scope**. The last argument of the spawn call, `ruleScope`, decides where that call's `@style` / `@layout` rules land:

| `ruleScope` | Rules registered into | Visible to |
|---|---|---|
| `ResolvedRuleScope::Local` (default) | a bucket owned by that mount | only the nodes from that mount |
| `ResolvedRuleScope::Global` | the document-level "component registration section" | every node in the document |

Usage:

- **Private per-page styles** (default): keep `Local`. Two pages can define `.title` differently without conflict.
- **One style sheet shared by several pages**: put it in a separate `theme.lui` containing only `@style` / `@layout` and no node tree, mount it once with `Global` (any existing parent works — the rules are registered even if the snippet has no content), then pages simply use `class="…"`.
- Cascade order: default rules < root-document `@style` < styles mounted with `Global` < `Local`. In other words **a page's own `.title` overrides the one in `theme.lui`**.

> Embedded works the same way: `EmbeddedHost` does not expose `ruleScope` directly, but `host.scene()` is the scene object, so the `ComponentInstantiator` call above works as-is. Alternatively put shared styles in the main `.lui`'s `@style` — that block is root-document global and visible to everything.

---

## 5. Platform Differences

The multi-file capability is identical across platforms; only "how files get into the program" differs.

### 5.1 Desktop

- Files live on disk; keep as many as you like. Read → mount or swap (§2 / §3).
- `ui/main.lui` is just the scaffold's shell name, not a rule. To use `ui/app.lui` + `ui/pages/*.lui`, change the path read in `src/MainScene.cpp` (the scaffold's `ReadUiSource` candidate list).
- The base for `~/` images is set by `ResourceManager::getInstance().setDocumentRoot(dir)`. If page files move into a subdirectory and contain `~/` paths, point `documentRoot` at that directory before mounting (or use ordinary paths relative to the exe instead of `~/`).

### 5.2 Android

- UI lives in `app/src/main/assets/ui/`; **any number of files** may be placed there (the whole `assets` directory is packed into the APK, see the jar step in `build.lpj`).
- Loading: `NativeBridge.loadUi("ui/home.lui")` can be called with a different path; from C++ use `AndroidRuntime::Instance().loadUiFromAsset("ui/settings.lui")`.
- One `loadUiFromAsset` is §3's "swap the whole UI". For §2's "shell + mount", read the asset text and hand it to `ComponentInstantiator` (get the scene via `loppe::Stage::getInstance().currentScene()`).

### 5.3 ESP32-S3

This step is the easiest to get wrong, so it gets its own section.

**`EMBED_FILES` accepts several files**, and symbol names are derived from the file path:

```cmake
# main/CMakeLists.txt
idf_component_register(
    SRCS ${APP_SOURCES}
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_spi esp_timer
    EMBED_FILES
        "ui/main.lui"          # → _binary_ui_main_lui_start / _end
        "ui/home.lui"          # → _binary_ui_home_lui_start / _end
        "ui/settings.lui"      # → _binary_ui_settings_lui_start / _end
)
```

Symbol rule: **every `/` in the path becomes `_`, then add `_start` / `_end`**. `main.lui` at the component root gives `_binary_main_lui_start`; `ui/home.lui` gives `_binary_ui_home_lui_start`. Files in subdirectories of the component must be listed with paths relative to the component directory.

```cpp
// One symbol pair per .lui: the type is "array", with an asm label binding the
// linker symbol to your variable name.
extern const uint8_t kMainLuiStart[]     asm("_binary_ui_main_lui_start");
extern const uint8_t kMainLuiEnd[]       asm("_binary_ui_main_lui_end");
extern const uint8_t kHomeLuiStart[]     asm("_binary_ui_home_lui_start");
extern const uint8_t kHomeLuiEnd[]       asm("_binary_ui_home_lui_end");

std::string uiText(const uint8_t* begin, const uint8_t* end)
{
    return std::string(reinterpret_cast<const char*>(begin),
                       static_cast<size_t>(end - begin));
}

// Startup: the shell
s_host.loadUi(uiText(kMainLuiStart, kMainLuiEnd));

// Mount a page (§2): the text comes from symbols too. Keep the returned
// ElementView — it is what you call remove() on to unmount.
loppe::ElementView page = loppe::ComponentInstantiator::SpawnElementViewUnderId(
    s_host.scene(), "page-slot", uiText(kHomeLuiStart, kHomeLuiEnd), &s_host.runtime());
```

- `s_host.loadUi(...)` **may be called repeatedly**: success swaps the UI, failure returns `false` and the screen keeps its current content.
- Mount with `s_host.runtime()` (the engine's `DocumentRuntime&`), not `nullptr`.
- Fonts: new Chinese characters used by a page must be added to `main/font_chars.txt` and `python tools/generate_font.py` re-run, otherwise those glyphs cannot be drawn (embedded has no freetype; glyphs come from the bitmap font).
- Every `.lui` embedded in the firmware costs flash; for many pages prefer §2 (one shell plus on-demand mounts) over mounting everything at once.
- List only `.lui` texts in `EMBED_FILES` (not `EMBED_TXTFILES`): the engine wants the raw bytes, without an IDF-appended `\0`.

---

## 6. Common Mistakes

| Symptom | Cause | Fix |
|---|---|---|
| Nothing appears after mounting | Typo in the mount point's `id`, or that `panel` has zero height | Check the `id`; the mount point and its parent chain need height (`flex-grow: 1` or explicit) |
| `.title` in two pages affects each other | Both files used `Global`, or either should have been `Local` | Keep page-private styles at the default `Local`; use `Global` only for genuinely shared styles (§4) |
| The same content appears twice | The same page was mounted twice without cleanup | Call `remove()` on the previously returned `ElementView` first (§2.4) |
| A button does nothing | `id` collides with another page and the selector hit the wrong node; or the event was bound before mounting | Keep `id` globally unique; bind after mounting |
| Parse error pointing at a marker such as `@ROWS@` | A template placeholder was written into the `.lui` (the DSL has no such syntax) | Delete the placeholder, keep the container empty and mount C++-built rows into it (§2.6) |
| Rows double up after a refresh | Mounted again without detaching the old batch | Keep the `ElementView` returned by `SpawnElementViewUnderId` and `remove()` it before re-mounting (§2.4, §2.6) |
| `_binary_...` symbols not found on embedded | The file is missing from `EMBED_FILES`, or the asm name does not match the path | Walk the path character by character: `/` → `_`, then `_start` / `_end` |
| Crash when using an old handle after a switch | The old control was reclaimed with the rebuilt tree | Re-fetch with `findControlById` after the switch (§3.1); guard `ElementView` with `if (view)` |
| `~/img/a.png` in a page does not load | `documentRoot` is not the directory of that `.lui` | Call `setDocumentRoot(that directory)` before mounting, or drop the `~/` form |

---

## 7. Checklist

- Where do UI files live? Desktop `ui/`, Android `app/src/main/assets/ui/`, ESP32 component directory (often `main/`).
- Is the project already split? Search for `SpawnElementViewUnderId`, repeated `loadUi`, several `.lui` files.
- Does the new page need chrome from the shell (top bar, sidebar, background)? Yes → §2 (mount); no → §3 (swap).
- Must its styles be shared with other pages? Yes → `Global` plus a separate `theme.lui`; no → keep `Local`.
- Are `id` / `class` names unique across all files? Is the mount point's `id` fixed?
- Any lists / repeated rows? Are there `@…@` placeholders left in the `.lui` (not even one, see §2.6)? Who builds the rows, and is `remove()` called before re-mounting?
- ESP32: is the new `.lui` listed in `EMBED_FILES`? Do the symbol names match? Are new Chinese characters in `font_chars.txt`?

---

## Related documents

- Getting Started (`LOPPE_GETTING_STARTED.md`) §4 — the three steps for loading a single `.lui` from disk.
- DSL User Guide (`LOPPE_DSL_USER_GUIDE.md`) §2 — file structure of a `.lui`.
- Layout Design (`LAYOUT_DESIGN.md`) — height chains, `overflow`, `flex-grow`.
- Built-in Controls (`LOPPE_BUILTIN_CONTROLS.md`) — properties and events of `panel` / `button` / `text`.
- `embedded/README.md` — `EmbeddedHost` setup, `loadUi`, and the main loop.
