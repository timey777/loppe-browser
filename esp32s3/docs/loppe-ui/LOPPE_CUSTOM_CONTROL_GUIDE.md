# LOPPE 自定义控件用户指南

> 📖 中文 | [English](LOPPE_CUSTOM_CONTROL_GUIDE_EN.md)

---

## 1. 本文讲什么

自定义控件指引擎内置标签覆盖不到的界面元素。本文讲如何在自己的工程里实现它们，共三种方式：

| 方式 | 做法 | 要写 C++ 吗 | 用到的 API |
|---|---|---|---|
| **方式一 组合已有控件** | 写一段 DSL 模板，把已有控件拼成新控件 | 否，只写挂载调用 | `UIComponent`、`ComponentInstantiator`、`ElementView` |
| **方式二 自绘控件类型** | 新增一个 DSL 标签，绘制与交互交给自己的 C++ 类 | 是 | `AbstractControl` 子类 + `ControlFactory` 子类 |
| **方式三 只改外观** | 结构与行为都用现成控件，只改样式与状态 | 否 | `@style`、`@layout`、class、伪状态选择器 |

三种方式可以叠加：方式二写出的控件，内部骨架可以用方式一的 DSL 模板搭，外观照样按方式三处理。

### 1.1 适用读者

使用 LOPPE UI 引擎开发界面的应用开发者。

### 1.2 前置条件

- 引擎包已接入工程：`lib/*.a` + `include/*.h`，且 `${pkg}/include` 在 include 列表里。
- 会写 `.lui` 界面。文件结构、选择器、样式与布局属性见 `LOPPE_DSL_USER_GUIDE.md`。
- 会用 `build.lpj` 维护工程的源文件列表与 include 目录。
- 本文中的**宿主**指你自己的工程：持有 `Scene`、监听事件、写业务逻辑的那一侧。

### 1.3 怎么选方式

按两个问题判断：

1. 这个控件能不能用已有控件拼出来（`panel` + 若干子控件 + class）？
   - 能 → 方式一。只是长相不同就用方式三，不必写 C++。
   - 不能 → 问第二个问题。
2. 是否需要自绘像素、自己算排版，或者接管键盘与滚轮？
   - 需要 → 方式二。
   - 不需要 → 方式三。

---

## 2. 如何用「组合已有控件」实现自定义控件（方式一）

适用场景：新控件是已有控件的组合，例如统计卡片、列表行、导航块。

### 2.1 步骤一：定义组件壳

```cpp
#include "components/ui_component.h"

class StatCard : public loppe::UIComponent {
public:
    void onAttach(loppe::Scene* scene, const std::string& parentId) override;
    // onDetach(scene) 默认为空实现；需要解绑宿主回调、停止后台线程时覆盖它
};
```

`UIComponent` 只有 `onAttach` / `onDetach` 两个钩子，不含其它逻辑；挂载动作由 §2.3 的 API 完成。

### 2.2 步骤二：写 DSL 模板

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

模板有两条要求：

- **只解析一次**。`AstSnippet` 可以反复实例化，每次挂载各自深拷贝、实例之间互不影响，所以 `static` 解析一次、挂 N 次是安全的。列表行必须这样做，不要每次挂载都重新 parse。
- **不写 `id`**。同一个模板实例化多次时 `id` 会重复；模板内一律用 class 定位，需要唯一 id 时由调用方注入，或在挂载后调 `setId`。

### 2.3 步骤三：把模板挂到父节点下

```cpp
#include "components/component_instantiator.h"

void StatCard::onAttach(loppe::Scene* scene, const std::string& parentId) {
    if (!scene) return;

    loppe::ElementView root = loppe::ComponentInstantiator::SpawnElementViewUnderId(
        scene, parentId, kStatCard, scene->getDocumentRuntime());
    if (!root) return;                       // 失败：parentId 不存在，或模板解析失败
}
```

四个重载 = `{DSL 字符串, AstSnippet}` × `{挂第一个顶层节点, 挂全部顶层节点}`：

```cpp
static ElementView              SpawnElementViewUnderId (Scene*, const std::string& parentId, const std::string& dsl, DocumentRuntime*, int index = -1, ResolvedRuleScope = Local);
static ElementView              SpawnElementViewUnderId (Scene*, const std::string& parentId, const AstSnippet&,      DocumentRuntime*, int index = -1, ResolvedRuleScope = Local);
static std::vector<ElementView> SpawnElementViewsUnderId(Scene*, const std::string& parentId, const std::string& dsl, DocumentRuntime*, int index = -1, ResolvedRuleScope = Local);
static std::vector<ElementView> SpawnElementViewsUnderId(Scene*, const std::string& parentId, const AstSnippet&,      DocumentRuntime*, int index = -1, ResolvedRuleScope = Local);
```

- 返回值就是成败判据：`if (!view)` 或 `views.empty()`；没有单独的 `bool` 版 API。
- `index` 是第一个顶层节点插入父节点子列表的位置：`-1` 追加到末尾，`0` 插到最前。
- `ResolvedRuleScope` 决定模板自带规则的生效范围，见 §2.7。

### 2.4 步骤四：用 ElementView 操作挂上去的元素

句柄内部只持 `weak_ptr`，元素被移除后自动失效，不会悬垂：

| 成员 | 说明 |
|---|---|
| `valid()` / `operator bool` | 元素是否还在 |
| `control()` | 底层 `AbstractControl*`；要调业务方法就 `dynamic_cast` 回具体类型 |
| `id()` / `uid()` | 标识 |
| `addClass` / `removeClass` / `hasClass` | 切换状态（优先用这个：外观交给样式块） |
| `remove()` | 从父容器摘除；结构当帧生效，内存帧末回收 |
| `ancestorByClass(cls)` / `descendantByClass(cls)` | 事件委托时找行、找子元素 |
| `ElementView::fromControl(ctrl)` | 由 `UIEvent::target` 取回句柄 |

### 2.5 步骤五：更新数据

列表类控件的增量对齐写法：只删该关的行、只追加新行、存活行只切 class：

```cpp
for (auto& r : rows) if (needClose(r)) { r.view.remove(); r.view = {}; }
for (auto& item : newItems)
    rows.push_back({ item, loppe::ComponentInstantiator::SpawnElementViewUnderId(scene, "list", kRow, ctx) });
```

「删光再建」是结构级编辑：重解析 DSL、重注册规则、重建控件树。实测（20 行列表，Release）：

| 刷新方式 | 每次切换耗时 |
|---|---|
| 删光重建 | ≈3.3ms |
| 预建好行、只换文本或切 class | ≈0.96ms |

行数不变时用「复用行 + 换数据/切 class」，只有行数真的变化时才增删元素。

### 2.6 步骤六：处理状态与交互

- 状态用 class 表达：宿主 `addClass("active")` / `removeClass("active")`，样式块里写 `.stat-card.active { ... }`。
- 交互态用已有伪状态选择器：`:hover`、`:active`、`:focus`、`:disabled`。
- 类切换若影响尺寸，用 `addClass(cls, /*forceLayout*/ true)`，或改完让父级重新布局。

### 2.7 步骤七：选择模板规则的生效范围

`SpawnElementView*` 的最后一个参数决定模板自带 `@style` / `@layout` 的注册范围：

- `ResolvedRuleScope::Local`（默认）：注册为**实例局部**规则，多个实例互不干扰。
- `ResolvedRuleScope::Global`：注册为文档全局，用于「拆成多个文件、或多次挂载共享同一套 CSS」的场景。

默认用 `Local`；只有在同一套 CSS 确实要跨多次挂载共享、且确认选择器不会串味时才用 `Global`。

---

## 3. 如何用「自绘控件类型」实现自定义控件（方式二）

适用场景：需要自绘像素、自己算排版，或者接管键盘与滚轮的控件，例如代码编辑器、树形视图、富文本。做法 = 一个 `AbstractControl` 子类 + 一个 `ControlFactory` 子类，共十一步。

### 3.1 步骤一：定义控件类

```cpp
#pragma once
#include <components/abstract_control.h>

namespace myapp {

class Dot : public loppe::AbstractControl {
public:
    std::string getTypeName() const override { return "dot"; }   // 必须等于 DSL 标签名

protected:
    void onRender(loppe::DisplayList& dl, const loppe::ui::Rect& clip) const override {
        (void)clip;
        dl.addRect(bounds_, getFillColor(), getStrokeColor(), getStrokeWidth(), getCornerRadius());
    }
};

} // namespace myapp
```

- `onRender` 与 `getTypeName` 是纯虚函数，必须实现；其余虚函数都有默认实现，按需覆盖。
- `getTypeName()` 的契约：忽略大小写等于 DSL 元素类型名（`dot:foo` 里的 `dot`）。不要返回 C++ 类名 —— 类型选择器 `dot { ... }` 按它匹配。
- `onRender` 收到的 `clip` 是引擎沿祖先链累积下传的裁剪区（本控件局部坐标），透传给绘制调用即可。

可覆盖的钩子及其时机：

| 成员 | 时机 | 职责 |
|---|---|---|
| `onRender(dl, clip)` | 每帧绘制（纯虚） | 只画；不要在这里标脏或重建布局 |
| `getTypeName()` | 随时（纯虚） | 返回 DSL 标签名 |
| `isContainer()` | 建控件树时 | 返回 `true` 才会把子节点建成子控件（只有容器需要） |
| `OnSyncFromResolvedNode(rn)` | 通用映射之后（含每次布局后） | 同步派生属性（见 §3.4） |
| `onLayoutSynced()` | bounds / viewport / scroll 同步完成后 | 按新尺寸更新自己的布局缓存（见 §3.5） |
| `handleLocalMouseButton/Move/Scroll(e)` | 命中到本控件时 | 用局部坐标做命中与拖拽（见 §3.6） |
| `onKey(e)` / `onChar(e)` | 本控件有焦点时 | 返回 `true` 表示已处理 |
| `onFocus()` / `onBlur()` | 焦点变化 | 更新自身状态、重置闪烁定时等 |
| `onHover(bool)` / `onActive(bool)` | 指针悬停 / 按下 | 只改状态并重绘，不做重活 |
| `update()` | 被排进动画队列的帧 | 按时间推进的逻辑（见 §3.8） |
| `onFrameCommit()` | 本帧 update 之后、paint 之前 | 补建本帧产生的内部脏布局（见 §3.8） |
| `invalidate()` | 你自己调用 | 「我变了」：标脏 + 请求重绘 + 下帧继续 tick |
| `requestAnimationTick()` | 你自己调用 | 只要求下帧继续 tick，不标脏、不重绘 |
| `markLayerDirty()` | 你自己调用 | 只把本控件标脏（视觉变了但不需要继续 tick） |
| `setScrollSize(w,h)` | 布局写入滚动尺寸时 | 自管滚动的控件覆盖成空实现，自己算（见 §3.7） |
| `setViewportSize(w,h)` | 布局写入视口时 | 通常先用基类实现；自己算完再调 `AbstractControl::setViewportSize` |

### 3.2 步骤二：实现绘制

`DisplayList` 提供的接口：

```cpp
dl.addRect(rect, fill, stroke = {}, strokeWidths = {}, cornerRadius = 0.0f);
dl.addText(text, rect, color, fontSize = 0.0f, fontFamily = "", layout = TextLayoutParams());
dl.addTextLayout(layout, rect, color, firstLine = 0, lineCount = 0);   // 自己排版文字时用
dl.addImage(source, rect);
dl.addSvgIcon(name, rect, tint = 白色);      // name = 已注册的图标名
dl.pushClip(rect /*, cornerRadius */); ... dl.popClip();
dl.pushTransform(dx, dy); ... dl.popTransform();
```

- `bounds_` 是本控件在父坐标系里的矩形，从它起步算内部几何。
- 自管滚动时先 `pushTransform(-scrollX, -scrollY)` 再画内容，并 `pushClip` 到自己的视口，否则内容会画到相邻控件上。
- 首帧或布局还没就绪时可以只画背景然后返回，不要在这里补布局。

### 3.3 步骤三：注册固有尺寸

DSL 里没写死宽度/高度时，布局引擎按节点类型查表测量固有尺寸，这张表是开放的：

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
            float w = 12.0f, h = 12.0f;                 // 该类型的固有默认尺寸
            if (ctx.textMeasurer) { /* 有文字内容时用 ctx.textMeasurer 测文字宽高 */ }
            if (requestedW == loppe::AUTO_SENTINEL) requestedW = w;   // 只填仍为 AUTO 的那一维
            if (requestedH == loppe::AUTO_SENTINEL) requestedH = h;
        });
}
```

- 只填仍为 `AUTO_SENTINEL` 的维度：显式写死的 `width` / `height` 由样式决定，不要覆盖。
- 不注册也能用：DSL 里显式写宽高，或让默认尺寸生效。
- 在构造函数里调一次 `ensureDotMeasureRegistered()` 即可（内部用 `static bool once` 保证幂等）。

### 3.4 步骤四：同步 DSL 属性与样式

每次布局或变更后，引擎先走一遍通用映射（bounds、scroll、viewport、`background-color`、`border`、`border-radius`、`padding`、`visible`、`background-image`），然后调用你的 `OnSyncFromResolvedNode(rn)`：

```cpp
void Dot::OnSyncFromResolvedNode(const loppe::ResolvedNode& rn) {
    // ① DSL 属性：dot(id="x", tone="danger", value="3")
    if (rn.astNode) {
        auto it = rn.astNode->attrs.find("tone");
        if (it != rn.astNode->attrs.end() && it->second.isString())
            tone_ = it->second.as<std::string>();
    }
    // ② 样式属性：@style / class 算出来的最终值
    const float fs = rn.getFinalStyle().fontSize;
    if (fs > 0.0f && fs != fontSize_) { fontSize_ = fs; layoutDirty_ = true; markLayerDirty(); }
}
```

三条要求：

1. **初始值只应用一次**：`value` 这类运行期会被宿主或用户改掉的属性，用一个 `bool initialized_` 守卫，否则下一次样式同步会把你已经改过的内容覆盖回去。
2. **影响尺寸的派生属性变化时要标脏**（`layoutDirty_ = true` + `markLayerDirty()`），否则画面不跟着变。
3. **通用外观不要再解析一遍**：背景、边框、圆角、内边距、可见性基类已经同步到 `getFillColor()` / `getStrokeColor()` / `getStrokeWidth()` / `getCornerRadius()` / `getPadding*()` / `isVisible()`，在 `onRender` 里取用即可。

### 3.5 步骤五：在布局钩子里更新缓存

`onLayoutSynced()` 在每次布局同步后都会调用（引擎逐帧对所有控件走一遍），所以：

- 应该做的：把新尺寸换算成自己的缓存（行高、视口、滚动条几何、内容尺寸）。
- 不要无条件重建排版：`layoutDirty_` 没脏就早退。写成无条件的代价很直接：每次拖分隔条都会重排整篇文本（230 行 ≈ 200ms/帧，1500 行 ≈ 1.35s/帧）。
- 如果新尺寸确实会影响内部排版，就在尺寸真的变了时才置脏，而不是每帧置脏。

### 3.6 步骤六：处理鼠标与键盘

```cpp
bool Dot::handleLocalMouseButton(const loppe::ControlLocalMouseButtonEvent& e) {
    if (!isVisible()) return false;
    if (e.action == GLFW_PRESS) {
        if (!hitTestLocal(e.point)) return false;
        toggle();                     // 控件自己的逻辑
        captureMouse();               // 需要拖动/拖选时捕获，结束后 releaseMouse()
        invalidate();
        return true;
    }
    if (e.action == GLFW_RELEASE) { releaseMouse(); return true; }
    return false;
}
```

- 事件里的点已经是本控件的局部坐标（换算由引擎完成）；判断「点在不在我身上」用 `hitTestLocal(point)`。
- 需要指针移出控件后继续收事件（拖拽、拖选）就 `captureMouse()` / `releaseMouse()`；需要别的参考坐标系时用 `captureMouse(relativeTo)`。
- 键盘走 `onKey` / `onChar`，只在有焦点时到达本控件；`onChar` 处理文本输入，`onKey` 处理功能键与组合键。
- 改指针形状：`getScene()->setCursor(Scene::Cursor_Hand /* 或 Arrow / IBeam */)`。

### 3.7 步骤七：自管滚动（需要时）

由控件自己决定能滚多远时（代码编辑器、自绘列表），把 `setScrollSize` 覆盖成空实现，自己算视口与内容尺寸：

```cpp
// 声明里：忽略布局给的内容尺寸
void setScrollSize(float w, float h) override { (void)w; (void)h; }

void MyView::applyScrollMetrics() {
    float vw = 0.0f, vh = 0.0f;
    textViewportSize(vw, vh);                            // 自己的内容区尺寸（已扣掉滚动条让位）
    setViewportSize(vw, vh);                             // 视口 = 可见窗口

    // 用基类名限定，绕过上面那个空实现，写入自己算出的内容尺寸
    loppe::AbstractControl::setScrollSize(contentW_, contentH_);

    // 内容变短或换数据后，把越界偏移钳回可滚动范围
    const float nx = std::max(0.0f, std::min(getScrollX(), getScrollWidth()  - vw));
    const float ny = std::max(0.0f, std::min(getScrollY(), getScrollHeight() - vh));
    if (nx != getScrollX() || ny != getScrollY()) { setScroll(nx, ny); markLayerDirty(); }
}
```

基类提供的滚动接口：`setScroll(x,y)` / `getScrollX|Y()`、`setScrollSize(w,h)` / `getScrollWidth|Height()`、`setViewportSize(w,h)` / `getViewportWidth|Height()` / `hasViewport()`、`styleScrollbarWidth()`（读取节点当前的 `scrollbar-width`）。

`hasViewport()` 只在 `overflow != visible` 时为真；不要拿它当裁剪开关 —— 裁剪由 `onRender` 的 `clip` 决定。

### 3.8 步骤八：重绘与时间推进

| 场景 | 用哪个 |
|---|---|
| 外观变了、只需一次重绘 | `markLayerDirty()`（只脏本控件）或 `invalidate()`（标脏 + 请求重绘 + 下帧 tick） |
| 按时间推进（光标闪烁、长按连发） | 在 `update()` 里判断；需要继续就 `requestAnimationTick()`，只有真的变了才 `invalidate()` |
| 内容或状态变化需要通知宿主 | 在 `update()` 里去重后上报（见 §3.9） |

两个常见错误：

- 每帧调 `invalidate()`：等于每帧整屏重绘。
- 只 `markLayerDirty()` 而没 `requestAnimationTick()`：控件不会被排进那一帧的 `update()`，「滚动位置变化上报给宿主」这类逻辑收不到通知（两侧同步滚动就是这么坏的）。

`update()` 里产生的内部脏布局，用 `onFrameCommit()` 兜底补建（它在 update 之后、paint 之前）；不补的话本帧 `onRender` 会走「布局没就绪」的早退分支，表现为丢帧闪烁。

### 3.9 步骤九：向宿主上报事件

控件内部状态变化后派发事件，宿主按选择器监听：

```cpp
void Dot::emitChange() {
    if (!getScene()) return;
    auto* ui = getScene()->getUIEventSystem();
    if (!ui) return;
    loppe::UIEventSystem::UIEvent e;
    e.type   = loppe::UIEventSystem::UIEventType::Change;   // 或 Input / Click 等
    e.target = this;
    e.text   = std::to_string(count_);
    ui->dispatch(e);
}
```

宿主侧（选择器支持 `#id` 与 `.class`）：

```cpp
scene->onChange("#unread", [](loppe::UIEventSystem::UIEvent& e) { /* e.text = 新值 */ });
scene->onClick(".row",    [](loppe::UIEventSystem::UIEvent& e) { /* e.deepestTarget = 实际被点的控件 */ });
scene->onInput("#code",   cb);
scene->onScroll("#list",  cb);
```

`UIEventType` 的取值：`Click`、`MouseDown`、`MouseUp`、`Input`、`Change`、`Scroll`、`Focus`、`Blur`、`KeyDown`、`KeyUp`。

- `e.target` 是当前派发到的控件（相当于 DOM 的 `currentTarget`）；`e.deepestTarget` 是命中的最深控件（相当于 DOM 的 `target`）。
- 处理器沿命中路径从最深的节点往上跑；置 `e.stopPropagation = true` 停止传播（同一节点上后续处理器也不再执行）。
- 事件量大时自己合并：在 `update()` 里比较上一次的值，变了才派发。
- **线程**：控件 API 只能在 UI 线程调用。后台线程（下载、git、索引）先 `UiDispatcher::post(...)` 回 UI 线程，再碰控件。

### 3.10 步骤十：定义工厂类并安装

DSL 里的 `dot` 标签需要有人把它变成 `Dot` 对象：

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
        return nullptr;      // 返回 nullptr 表示交回内置标签（panel/text/input… 照常工作）
    }
};
} // namespace myapp
```

`doMakeControlFromResolvedNodeHook` 是新增 DSL 标签的唯一入口：建控件时先问它，返回空才落到内置标签（`panel` / `text` / `button` / `input` / `combobox` / `radio` / `progress` / `icon` / `image` / `Root`）的默认实现。

安装时机：在文档解析、控件创建之前装一次（宿主启动期），例如 `Application::build()` 里：

```cpp
loppe::ControlFactory::setInstance(std::make_unique<myapp::MyControlFactory>());
```

- 装得太晚（控件树已经建好）→ 那些节点没有控件，界面空白。
- 一个进程装一次即可；集成测试里要临时换工厂时，成对使用 `ControlFactory::releaseInstance()` + `setInstance(...)`，用完还原。

一个工程可以有多个自定义控件，都走同一个工厂，hook 里按 `rn->astNode->type` 分派即可。要在已有自定义类型之上追加自己的类型，在构造函数里接一个 base 工厂并转交：

```cpp
class MyFactory : public loppe::ControlFactory {
public:
    explicit MyFactory(std::unique_ptr<loppe::ControlFactory> base) : base_(std::move(base)) {}

    std::shared_ptr<loppe::AbstractControl> doMakeControlFromResolvedNodeHook(loppe::ResolvedNode* rn) override {
        if (rn && rn->astNode && rn->astNode->type == "dot") return std::make_shared<Dot>();
        if (base_) return base_->doMakeControlFromResolvedNodeHook(rn);   // 先问被包装的那一层
        return nullptr;                                                   // 再交回内置标签
    }
private:
    std::unique_ptr<loppe::ControlFactory> base_;
};
```

### 3.11 步骤十一：换掉引擎自动创建的滚动条外观（可选）

```cpp
std::shared_ptr<loppe::ScrollbarControl> CreateScrollbar(loppe::ScrollbarControl::Orientation o) override {
    return std::make_shared<MyThinScrollbar>(o);   // 必须是 ScrollbarControl 的子类
}
```

粗细由滚动条自己按宿主节点的 `scrollbar-width` 样式现读，不需要宿主接线。

### 3.12 自建子控件（需要私有辅助控件时）

滚动条这类由控件自己拥有的辅助控件：用 `shared_ptr` 持有、把自己设为父节点、注入 Scene，事件自己转发（它们不在布局树里，命中路径不会带上它们）：

```cpp
#include <components/scrollbar_control.h>   // loppe::ScrollbarControl

void ensureScrollbars() {
    if (!sbV_) { sbV_ = std::make_shared<MyScrollbar>(loppe::ScrollbarControl::Orientation::Vertical);
                 sbV_->setParent(this); }
    if (getScene() && sbV_->getScene() != getScene()) sbV_->setScene(getScene());
}
```

- 命中判定用 parent-local 坐标（`makeParentLocalPoint(point.x.value, point.y.value)`），与滚动条自己的车道矩形同源。
- 鼠标移动要手动转发给滚动条（`onParentLocalMouseMove`），指针离开本控件时清掉它们的 hover 态。

---

## 4. 如何用「只改外观」实现自定义控件（方式三）

适用场景：结构与行为都用现成控件，只是长相、状态不同。

1. **搭结构**：用 `panel` / `text` / `icon` / `image`（或方式一的 DSL 模板）搭出所需结构，也可以直接用现成控件。
2. **状态用 class**：宿主 `addClass("active")` / `removeClass("active")`，样式块里写 `.stat-card.active { ... }`。
3. **交互态用伪状态选择器**：`:hover`、`:active`、`:focus`、`:disabled`。
4. **图标**用已注册的 SVG 名（`icon(name="...")`），新图标需要先注册。
5. **类切换若影响尺寸**，用 `addClass(cls, /*forceLayout*/ true)`，或改完让父级重新布局。

外观留在 `.lui` 与样式块里，主题切换、i18n、换肤都不需要重新编译。

---

## 5. 完整示例：自绘 `badge`

**1) 控件**（`components/badge.h` / `badge.cpp`）

```cpp
// badge.h
#pragma once
#include <components/abstract_control.h>
#include <string>

namespace myapp {

// 自绘角标：圆角底 + 居中数字；点击派发 Click 事件给宿主。
class Badge : public loppe::AbstractControl {
public:
    std::string getTypeName() const override { return "badge"; }

    int value() const { return value_; }
    void setValue(int v) { if (v == value_) return; value_ = v; layoutDirty_ = true; markLayerDirty(); }

    static void ensureMeasureRegistered();   // 固有尺寸

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
    if (!initialized_ && rn.astNode) {         // 初值只应用一次
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

**2) 工厂**（`components/my_control_factory.h`）：见 §3.10，把 `"dot"` 换成 `"badge"`。

**3) 安装**（宿主 `Application::build()` 里，一次）：

```cpp
loppe::ControlFactory::setInstance(std::make_unique<myapp::MyControlFactory>());
```

**4) DSL 用法**：

```loppe
@style {
    .unread     { background-color:#e5484d; border-radius:10px; }
    .unread.zero{ background-color:#8b8f96; }      /* 状态用 class 表达 */
}
badge(id="unread", class="unread", value=3)
```

**5) 宿主响应**：

```cpp
scene->onClick("#unread", [](loppe::UIEventSystem::UIEvent&) { /* 打开消息面板 */ });
```

**6) 接线**：`badge.cpp` 加进 `build.lpj` 的 `sources`；`components/` 已在 include 列表里则无需再动。

---

## 6. 构建接线（新增文件时）

新增控件类 = 新增 `.cpp/.h`，源文件列表是显式的，不会自动收集：

1. 放进工程的合适子目录（例如 `components/`）。
2. 加进 `build.lpj` 的 `sources`（有些工程把共享列表单列成 `shared = [ ... ]`、被各 target 引用，加在那一份里）。
3. 只写一份构建描述：工程用 `build.lpj`，不新增 `CMakeLists.txt`（构建器不读它）。
4. 新增子目录时，include 目录也要一起加（`include = [ ., components, ... ]`）。
5. 引擎包已提供 `<components/*.h>`、`<engine/*.h>`、`<render/*.h>` 等公开头文件，照着现有 include 列表引用即可。

构建命令：在工程目录 `loppe-cli build . Debug`（要单编某个目标就加 `--targets=<目标名>`，目标名取自你的 `build.lpj`）。
命令、参数与产物位置见《用 loppe-cli 构建与运行》（`LOPPE_CLI_GUIDE.md`）。

---

## 7. 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| DSL 里写了标签，界面空白 | 宿主没装 `ControlFactory`，或装得太晚；hook 没认这个 `type` | 在启动期 `setInstance`；hook 里按 `type` 分派 |
| 类型选择器 `dot { ... }` 不生效 | `getTypeName()` 返回了 C++ 类名，或与标签的大小写、拼写不一致 | 返回 DSL 标签名 |
| 改了内部状态画面不变 | 只改了成员，没有 `markLayerDirty()` / `invalidate()` | 改完标脏 |
| 每帧重排、拖动卡顿 | `onLayoutSynced()` 里无条件重建布局 | 先查 `layoutDirty_` |
| 光标闪烁、CPU 一直高 | `update()` 里每帧 `invalidate()` | 用 `requestAnimationTick()`，只有真的变了才 `invalidate()` |
| 滚了一边，另一边不跟着滚 | 改滚动后控件没被排进下一帧 tick，宿主收不到上报 | 同时 `invalidate()` 或 `requestAnimationTick()` |
| 文字/图形画到相邻控件上 | `onRender` 里没 `pushClip` 到自己的视口 | 先 `pushClip` 再画 |
| 长文本把卡片撑破 | 内容尺寸与视口口径不一致 | 见布局文档的盒模型与溢出章节 |
| 长按删除时文字闪烁 | 本帧 `update()` 里产生的脏布局没在 `onFrameCommit()` 里补建 | 在 `onFrameCommit()` 里补建 |
| 列表越切越卡 | 用「删光重建」刷新列表 | 复用元素 + 换数据/切 class |
| 模板实例化多次后样式或事件串了 | 模板里写死了 `id`，或该用 `Local` 却用了 `Global` | 模板 id-less；作用域用 `Local` |
| 跨线程改控件崩溃 | 后台线程直接碰控件 | `UiDispatcher::post` 回 UI 线程 |

---

## 8. 交付前自查清单

自绘控件（方式二）交付前按这张表过一遍：

| 项目 | 检查点 |
|---|---|
| 固有尺寸 | 只填仍为 `AUTO_SENTINEL` 的维度；多个标签共用一套测量策略也别重复注册 |
| 属性同步 | 初值用 `initialized_` 只应用一次；只有真正影响尺寸/排版的属性变化才置脏 |
| 布局钩子 | `onLayoutSynced()` 先查脏标记；重排只在尺寸真的变了时发生 |
| 滚动 | 自算内容尺寸 + 视口 + 滚动条几何；内容变短后把偏移钳回可滚范围 |
| 输入 | 局部坐标 + `hitTestLocal`；拖拽/拖选用 `captureMouse` / `releaseMouse`；指针形状按区域切换 |
| 时间推进 | `update()` 里判断，需要继续才 `requestAnimationTick()`；只在真的变了才 `invalidate()` |
| 帧内补建 | `update()` 里产生的内部脏布局 → `onFrameCommit()` 兜底，避免丢帧闪烁 |
| 事件上报 | `update()` 里去重后才派发；宿主按 `#id` / `.class` 监听 |
| 绘制 | `pushClip` 到自己的视口 + `pushTransform(-scrollX, -scrollY)`；布局未就绪只画背景 |
| 接线 | 新文件进了 `build.lpj` 的 `sources`；新目录进了 include 列表 |

---

## 相关文档

| 文档 | 内容 |
|---|---|
| `LOPPE_DSL_USER_GUIDE.md` | DSL 语法、样式、布局属性（写模板与界面必读） |
| `ai/LOPPE_DSL_AI_REFERENCE.md` | 属性全表 + 与标准 CSS 的关键差异（AI 用） |
| `LAYOUT_DESIGN.md` | 尺寸模型、盒模型、滚动容器 |
| `ai/LOPPE_CUSTOM_CONTROL_AI_PROMPT.md` | 交给 AI 生成自定义控件时的提示词模板与硬约束（AI 用） |
