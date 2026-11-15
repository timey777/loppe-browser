# 一个工程里的多份 `.lui`

> 📖 中文 | [English](LOPPE_MULTI_UI_EN.md)

本文讲清一件事：**一个工程不必只有一份 `.lui`，也常常不该只有一份。**

适用读者：已经能跑起一个单 `.lui` 的工程（桌面 / Android / ESP32-S3），想把界面拆开的人，
以及按文档生成代码的 AI 助手。前置条件：知道 `.lui` 由 `@style` / `@layout` / 节点树三段组成
（见《LOPPE DSL 用户指南》），知道怎么把一个 `.lui` 文件交给引擎（见《入门指南》§4，
嵌入式见 `embedded/README.md` 的 `EmbeddedHost`）。

内容导览：§1 三种组织方式与选择；§2 拆成「外壳 + 若干界面」的做法；§3 每次换界面的做法；
§4 多个文件共享样式；§5 各平台的接线差异；§6 常见错误；§7 判断清单。

---

## 1. 三种组织方式

一份 `.lui` 里的节点树就是一棵控件树。要在工程里放下更多界面，只有三种合法做法：

| 方式 | 结构 | 什么时候用 | 代价 |
|---|---|---|---|
| A. 单文件分区 | 一份 `.lui`，用 `panel` 分区，靠 `class` / `display` 控制显隐 | 界面很小时 | 文件一长就难改；所有界面共用一个 `@style` 命名空间 |
| B. 外壳 + 若干界面文件 | 一份外壳 `.lui` 留挂载点（`id="…"`），每个界面一份 `.lui`，运行期挂到挂载点下 | **默认推荐**：多界面、面板、可复用区块 | 需要写一点 C++（读文件 + 挂载） |
| C. 每次整份换掉 | 每个界面一份完整 `.lui`，切换时把整份交给引擎重新解析 | 界面之间没有共用外壳（例如全屏引导页、独立工具页） | 每次切换整棵重建，瞬时开销比 B 大 |

**引擎没有「import 另一个 `.lui`」这类语法**：`.lui` 之间不会互相 `@include`，拼接发生在 C++
侧——读出多份文本，再分别交给引擎。下面 §2 / §3 就是这句话的两种落地。

> **给 AI 的提醒**：拿到一个工程时不要默认「界面只有 `main.lui` 一份」。先看界面目录里
> 有几份 `.lui`（桌面 `ui/`、Android `app/src/main/assets/ui/`、ESP32 工程 `main/`），
> 再看 C++ 里有没有 `SpawnElementViewUnderId` / 多次 `loadUi` —— 已经拆过的工程，
> 新增界面应当**新增一份 `.lui` 并挂载**，而不是把内容全塞进 `main.lui`。

---

## 2. 拆成「外壳 + 若干界面」

这是编辑器自己用的组织方式，也是多界面工程最省事的形态：一份**外壳**只放共用部分
（顶栏、侧栏、状态栏）并给「会变的那块」留一个锚点；每个界面一份 `.lui`，运行期挂到锚点下。

### 2.1 外壳里留挂载点

```loppe
@style {
    .app    { width: 100%; height: 100%; background-color: #0f141c; padding: 12px; }
    .header { height: 44px; background-color: #1b2431; border-radius: 6px; }
    .slot   { width: 100%; flex-grow: 1; }        /* 会变的那块：占满剩余高度 */
}
@layout {
    .app { display: flex; flex-direction: column; }
}

panel(class="app") {
    panel(class="header") { text(value="标题栏，所有界面共用") },
    panel(id="page-slot", class="slot")           /* ← 挂载点：这里现在是空的 */
}
```

挂载点就是一个**带 `id` 的空 `panel`**。给它 `id` 是为了运行期能按 id 找到它；给它
`flex-grow: 1` 是为了让被挂上来的界面拿到剩余空间。注意容器 `overflow` 默认是 `auto`，
高度链要逐层写全（见 `LAYOUT_DESIGN.md`）。

### 2.2 每个界面一份 `.lui`

`ui/home.lui`：

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
    text(class="title", value="首页"),
    button(id="btn-go-settings", class="btn") { text(value="去设置") }
}
```

`ui/settings.lui` 同构。要点：

- 每份界面文件里**可以有自己的 `@style` / `@layout`** —— 它们只作用于挂在这次挂载下的节点
  （默认 `ResolvedRuleScope::Local`，见 §4）。
- 界面文件**不要重复写外壳已经给的东西**（背景、整页尺寸、顶栏）。它的根节点只描述「这一块」。
- 文件里用到 `~/` 相对路径（背景图）时，基准是**最近一次告诉引擎的那个目录**（桌面端
  `ResourceManager::setDocumentRoot`）。界面文件与外壳同目录时不用管；分到子目录时见 §5。

### 2.3 挂到挂载点上

```cpp
#include "components/component_instantiator.h"   // ComponentInstantiator
#include "components/element_view.h"             // ElementView（卸载用）
#include "components/scene.h"

// 读一份 .lui 文本（路径按你的平台取；桌面端可以直接用 std::ifstream）
std::string readText(const std::string& path);

// 当前挂着的界面。挂载返回的 ElementView 就是卸载凭据，留着它 —— 见 §2.4
loppe::ElementView g_page;

void showPage(loppe::Scene* scene, const std::string& luiPath)
{
    g_page.remove();                                  // 先摘掉上一个界面（空句柄是空操作）
    g_page = loppe::ComponentInstantiator::SpawnElementViewUnderId(
        scene, "page-slot", readText(luiPath), scene->getDocumentRuntime());
    if (!g_page) {
        // 挂载失败：父节点不存在，或这段 DSL 解析失败
    }
}
```

`SpawnElementViewUnderId` 把一段 DSL 当模板实例化，顶层内容节点成为 `page-slot` 的子节点；
返回的 `ElementView` 用 `if (...)` 判成败即可。切换界面就是「摘掉旧的 + 挂上新的」两句。

### 2.4 卸载与重建

- **卸载用 `ElementView::remove()`**：把 `SpawnElementViewUnderId` 的返回值存下来，换界面前调它。
  结构当帧摘除（本帧布局、命中就看不见了），内存由引擎在帧末回收，不用自己 delete。
- 存一份「当前界面」句柄就够了：`remove()` 摘的是整棵挂载子树，界面里的子控件不用逐个管。
- 重复挂载同一份界面 = 界面上出现两份。**先 remove 再挂**。
- 界面文件里的 `id` 在**整棵文档**里必须唯一（见 §6）。

要成批管理（列表行、可复用的卡片集合）时引擎还有 `ComponentHost`：`Mount()` 挂一次模板、
`SetRows()` 按稳定 key 对账行、`Unmount()` 一次摘干净，比手拼 spawn/remove 省事
（见 `components/component_host.h`，用法对比见 §2.6）。

### 2.5 事件绑定写在 C++ 侧

挂载出来的控件同样可以按选择器绑事件：

```cpp
scene->onClick("#btn-go-settings", [scene](loppe::UIEventSystem::UIEvent&) {
    showPage(scene, "ui/settings.lui");
});
```

选择器只支持 `type` / `.class` / `#id` 及其同元素组合，**没有后代选择器**——所以界面文件之间的
`id` / `class` 不要重名，否则一条规则会命中好几处。

### 2.6 列表行：不要在 `.lui` 里写 `@占位@`

「先摆好行的样子，数据来了往里填」这种需求，很容易写成在 `.lui` 里留一个标记，C++ 里再做字符串替换：

```loppe
panel(class="node-grid") {
    @ROWS@                 // ❌ 解析报错
}
```

**`.lui` 没有模板 / 占位 / 循环语法。** `@` 只在 `@style` / `@layout` / `@version` 三个块头出现，
`@ROWS@` 会被当成未知块头，解析直接失败。这份文件在编辑器里打开就报错，运行期 `parse` 也过不去。

即使把替换做在 `parse` 之前、运行期侥幸不报错，也不该这么写：文件本身是坏语法，编辑器打不开、改不了；
替换是纯文本的，字段值里带一个 `"` 或换行就会把整段 DSL 拼坏。

列表行的做法按规模选一种，三种都不需要占位符：

| 规模 | 做法 | 先例 |
|---|---|---|
| 几十行、结构固定 | 容器留空 `panel(id="rows-host") {}`，C++ 拼好整段行 DSL 挂上去；换数据时先 `remove()` 旧的 | 编辑器欢迎页「最近打开」（`editor/panels/welcome_panel.cpp` 的 `refreshWelcomeList`） |
| 成批行、要按 key 增删 | `ComponentHost`：`Mount()` 挂模板一次，`SetRows()` 按稳定 key 对账 | `components/component_host.h` |
| 几百上千行、树形 | 自绘控件（自己画行，不逐行造控件） | 编辑器文件树 `editor/components/tree_view.cpp` |

第一种最常用，形状是这样：

```loppe
// ui/pages/proxies.lui —— 容器空着，本身是合法 DSL
panel(class="node-grid", id="rows-host") {}
```

```cpp
// 数据到了再拼行，挂到容器下
std::string rows;
for (size_t i = 0; i < nodes.size(); ++i) {
    if (i) rows += ",";
    rows += "panel(class=\"node-card\", id=\"n" + std::to_string(i) + "\") {"
            " text(class=\"node-name\", value=\"" + Esc(nodes[i].name) + "\") }";
}
rowsView_.remove();                       // 旧行先摘，避免出现两份
rowsView_ = ComponentInstantiator::SpawnElementViewUnderId(this, "rows-host", rows,
                                                           getDocumentRuntime());
```

几个必须注意的点：

- **值要转义**：`"` 和 `\` 前面加反斜杠（`Esc()` 就干这件事）。引擎没有导出这个 helper，宿主自己写十行。
- **行 DSL 不带 `@style`**：每行注册一份 CSS 会一直堆着。样式写在页面文件的 `@style` 里，行只引用 class；
  用 `ComponentHost::SetRows` 时，行引用的 class 由 `Mount()` 的模板提供。
- **行内事件用类委托绑一次**（`scene->onClick(".node-card", ...)`），不要逐行绑闭包——行重建之后
  旧闭包还留在事件系统里。

---

## 3. 每次换掉整份界面

界面之间没有共用外壳时（全屏引导页、独立的工具页），把每份 `.lui` 都写成完整的界面，
切换时整份交给引擎：

```cpp
// 桌面端：重新 parse + notifyASTUpdated（入门指南 §4 说的「改完 .lui 立刻生效」就是这条）
bool loadDocument(const std::string& path);   // 见《入门指南》§4 的加载三步

// 嵌入式：EmbeddedHost 可以直接再喂一份
host.loadUi(readText("main/boot.lui"));
```

- 桌面 / Android：`LoppeParser::parse` → `notifyASTUpdated(ast)`。引擎会按节点身份做差量，
  内容相同的部分不会整体重建。
- 嵌入式：`EmbeddedHost::loadUi(...)` 可重复调用，解析失败时屏幕上保持原样（不会黑屏）。
- 走这条路的界面**要自带完整样式**（背景、整页尺寸），因为没有外壳兜底。

---

## 3.1 换界面时注意

- **切换是整棵重建**：被换掉的界面上的控件句柄（`findControlById` 拿到的那个）随之失效，
  切换后要重新取。
- **绑过一次的事件不用重绑**：`onClick("#id", …)` 是按选择器绑在场景上的，不是绑在控件指针上；
  控件重建后新控件仍会命中同一个选择器。前提是 `id` 稳定、不重名。
- **首帧会整屏重画**：嵌入式端结构变化会产生大块脏区，`EmbeddedHost` 会自行退化成整屏刷。

---

## 4. 多个文件共享样式

`.lui` 之间不 import，所以「共用的一份样式」要靠**作用域**解决。挂载时的最后一个参数
`ruleScope` 决定这次挂载的 `@style` / `@layout` 规则落在哪：

| `ruleScope` | 规则被注册到 | 谁看得见 |
|---|---|---|
| `ResolvedRuleScope::Local`（默认） | 这次挂载自己的桶 | 只有这次挂载出来的节点 |
| `ResolvedRuleScope::Global` | 文档级的「组件注册段」 | 文档里所有节点 |

用法：

- **每个界面的私有样式**（默认）：留 `Local`。同名的 `.title` 在两个界面里写不同样式也不会打架。
- **多个界面共用的一份样式**：单独放一份 `theme.lui`，只写 `@style` / `@layout`、不写节点树，
  用 `Global` 挂一次（挂在任意已存在的节点下、内容是空的也可以——规则照样注册），之后各界面
  直接写 `class="…"` 就能命中。
- 层叠优先级：默认规则 < 根文档 `@style` < 用 `Global` 挂进来的共用样式 < `Local`。
  也就是说**界面文件自己的 `.title` 会盖住 `theme.lui` 里的 `.title`**。

> 嵌入式端同理：`EmbeddedHost` 没有直接暴露 `ruleScope`，但 `host.scene()` 就是场景对象，
> 可以照上面的写法用 `ComponentInstantiator` 挂载；也可以把共用样式直接写在主 `.lui` 的
> `@style` 里（那份是根文档全局样式，人人可见）。

---

## 5. 各平台的接线差异

多份 `.lui` 的能力在引擎里是同一套，差别只在「文件怎么进到程序里」。

### 5.1 桌面

- 文件在磁盘上，随便放几份。读文件 → 挂载或整份切换（§2 / §3）。
- `ui/main.lui` 只是模板给的外壳名字，不是规定。想改成 `ui/app.lui` + `ui/pages/*.lui`，
  改 `src/MainScene.cpp` 里读文件的路径即可（模板里是 `ReadUiSource` 的候选列表）。
- `~/` 图标的基准由 `ResourceManager::getInstance().setDocumentRoot(目录)` 给定。界面文件挪到
  子目录后，若里面有 `~/` 路径，需要在挂载前把 `documentRoot` 指向它所在目录（或改成相对
  exe 的普通路径，别用 `~/`）。

### 5.2 Android

- 界面放在 `app/src/main/assets/ui/`，**可以放任意多份**（`assets` 整个目录会被打进 APK，
  见 `build.lpj` 的 jar 打包步骤）。
- 加载：`NativeBridge.loadUi("ui/home.lui")` 可以换路径调；也可以在 C++ 侧
  `AndroidRuntime::Instance().loadUiFromAsset("ui/settings.lui")`。
- 一次 `loadUiFromAsset` 是 §3 的「整份换掉」。要 §2 的「外壳 + 挂载」，读 assets 文本后交给
  `ComponentInstantiator` 即可（`scene` 从 `loppe::Stage::getInstance().currentScene()` 取）。

### 5.3 ESP32-S3

这一步最容易走错，单独说。

**`EMBED_FILES` 可以列多份**，符号名按文件名生成：

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

符号规则：**路径里的 `/` 换成 `_`，再加 `_start` / `_end`**。`main.lui`（在组件根）是
`_binary_main_lui_start`；`ui/home.lui` 是 `_binary_ui_home_lui_start`。文件放在组件目录的子目录里，
组件注册时路径要写相对组件目录的路径。

```cpp
// 每份 .lui 一对符号：类型是「数组」，用 asm 标签把链接器符号绑到变量名上
extern const uint8_t kMainLuiStart[]     asm("_binary_ui_main_lui_start");
extern const uint8_t kMainLuiEnd[]       asm("_binary_ui_main_lui_end");
extern const uint8_t kHomeLuiStart[]     asm("_binary_ui_home_lui_start");
extern const uint8_t kHomeLuiEnd[]       asm("_binary_ui_home_lui_end");

std::string uiText(const uint8_t* begin, const uint8_t* end)
{
    return std::string(reinterpret_cast<const char*>(begin),
                       static_cast<size_t>(end - begin));
}

// 启动：外壳
s_host.loadUi(uiText(kMainLuiStart, kMainLuiEnd));

// 挂载某个界面（§2）：文本同样来自符号；返回值留着，卸载时调它的 remove()
loppe::ElementView page = loppe::ComponentInstantiator::SpawnElementViewUnderId(
    s_host.scene(), "page-slot", uiText(kHomeLuiStart, kHomeLuiEnd), &s_host.runtime());
```

- `s_host.loadUi(...)` **可以重复调用**：解析成功就换界面，失败返回 `false` 且屏幕保持原样。
- 挂载要用 `s_host.runtime()`（引擎的 `DocumentRuntime&`），不是 `nullptr`。
- 字库：界面里用到的新汉字要写进 `main/font_chars.txt` 再跑 `python tools/generate_font.py`，
  否则这些字画不出来（嵌入式没有 freetype，字形来自位图字库）。
- 一份 `.lui` 编进固件会占 Flash；界面很多时优先用 §2（一份外壳 + 按需挂载），别把所有界面
  一次性挂上去。
- `EMBED_FILES` 里只列 `.lui` 文本（不是 `EMBED_TXTFILES`）：引擎要原始字节，不需要 IDF 补 `\0`。

---

## 6. 常见错误

| 现象 | 原因 | 处理 |
|---|---|---|
| 挂载后看不到任何东西 | 挂载点的 `id` 拼错 / 该 `panel` 高度为 0 | 核对 `id`；挂载点与它的父链都要有高度（`flex-grow: 1` 或具体高度） |
| 两个界面的 `.title` 互相影响 | 两份文件用了同一 `ruleScope`（`Global`），或本来就该用 `Local` | 界面私有样式留默认 `Local`；只有真的要共用才用 `Global`（§4） |
| 界面上出现两份一样的内容 | 同一份界面挂了两次没清 | 挂之前对上一次返回的 `ElementView` 调 `remove()`（§2.4） |
| 点了按钮没反应 | `id` 与另一份界面重名，选择器命中了别处；或绑定时控件还没挂上 | `id` 全局唯一；事件在挂载之后绑 |
| `.lui` 解析报错，报的位置是 `@ROWS@` 这类标记 | 在 `.lui` 里写了模板占位符（DSL 没有这种语法） | 删掉占位符，容器留空，行由 C++ 生成后挂上去（§2.6） |
| 列表刷新后行数翻倍 / 旧行还在 | 又挂了一次没摘旧的 | 保存 `SpawnElementViewUnderId` 的返回值，重挂前 `remove()`（§2.4、§2.6） |
| 嵌入式的 `_binary_...` 找不到符号 | `EMBED_FILES` 里没列这份文件，或 asm 名字与实际路径不符 | 按「路径 `/` → `_`，加 `_start`/`_end`」逐字符核对 |
| 切换界面后调旧句柄崩溃 | 旧控件已随整棵重建被回收 | 切换后重新 `findControlById`（§3.1）；`ElementView` 用 `if (view)` 先判有效 |
| 界面里的 `~/img/a.png` 加载不到 | `documentRoot` 指向的不是该 `.lui` 所在目录 | 挂载前 `setDocumentRoot(该目录)`，或改用不带 `~/` 的路径 |

---

## 7. 判断清单

- 界面文件在哪个目录？桌面 `ui/`、Android `app/src/main/assets/ui/`、ESP32 组件目录（常是 `main/`）。
- 现有工程已经拆过吗？搜 `SpawnElementViewUnderId` / 多次 `loadUi` / 多份 `.lui`。
- 新界面需要外壳给的东西（顶栏、侧栏、背景）吗？需要 → §2（挂载），不需要 → §3（整份换）。
- 它的样式要跟别的界面共用吗？要 → `Global` + 单独的 `theme.lui`；不要 → 保持 `Local`。
- 每份文件的 `id` / `class` 是否全局唯一？挂载点的 `id` 是否固定？
- 有列表 / 重复行吗？`.lui` 里有没有 `@…@` 这类占位符（一个都不能留，见 §2.6）？行是谁生成的、重挂前有没有 `remove()`？
- ESP32：新 `.lui` 写进 `EMBED_FILES` 了吗？符号名对得上吗？新汉字进 `font_chars.txt` 了吗？

---

## 相关文档

- 《入门指南》（`LOPPE_GETTING_STARTED.md`）§4 —— 单个 `.lui` 从磁盘加载的三步。
- 《LOPPE DSL 用户指南》（`LOPPE_DSL_USER_GUIDE.md`）§2 —— `.lui` 文件结构。
- 《布局设计》（`LAYOUT_DESIGN.md`）—— 高度链、`overflow`、`flex-grow`。
- 《内置控件》（`LOPPE_BUILTIN_CONTROLS.md`）—— `panel` / `button` / `text` 的属性与事件。
- `embedded/README.md` —— `EmbeddedHost` 的装配、`loadUi` 与主循环。
