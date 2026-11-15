# LOPPE 自定义控件 AI 提示词（AI 版）

> 紧凑版，用于注入 AI 上下文：硬约束、方式决策、API 契约、代码骨架、可粘贴提示词模板。
> 配套长文：[自定义控件用户指南](../LOPPE_CUSTOM_CONTROL_GUIDE.md)。

> 📖 中文 | [English](LOPPE_CUSTOM_CONTROL_AI_PROMPT_EN.md)

---

## 1. 硬约束（AI 必须遵守，违反即错）

1. **代码只写在自己的工程里**：引擎以预编译包（`lib/*.a` + `include/*.h`）提供，
   你只能用公开头文件里的 API，不要臆造引擎内部接口。
2. **只走三个扩展点**：组合式挂载（`ComponentInstantiator`）、新控件类型（`ControlFactory` 子类）、
   外观/状态（DSL `@style` + class）。不要发明第四个。
3. **新标签必须同时给两样**：`AbstractControl` 子类（`getTypeName()` 返回**DSL 标签名**）
   + `ControlFactory` 子类（hook 里认这个 `type`），否则 DSL 里写了标签也不出控件。
4. **工厂在启动期装一次**：`ControlFactory::setInstance(...)`，早于文档解析/控件创建。
5. **只填 `AUTO_SENTINEL`**：固有尺寸测量里不覆盖样式显式写死的宽高。
6. **不硬编码 id**：可复用的 DSL 模板写成 id-less（多次实例化会撞 id）。
7. **UI 线程**：后台线程先 `UiDispatcher::post(...)` 再碰控件。
8. **渲染期只画**：`onRender` 里禁止标脏/重建布局。
9. **新文件要接线**：加进 `build.lpj` 的 `sources`、新目录补 `include`（工程只有 `build.lpj` 一份构建描述，
   没有 `CMakeLists.txt`；构建统一 `loppe-cli build . Debug`，见 `../LOPPE_CLI_GUIDE.md`）。
10. **不要自己复刻引擎已有能力**：背景/边框/圆角/padding/可见性/滚动条/文本编辑/双击三击选词
    都用引擎给的（基类映射 + 共享组件），不要另写一套。

---

## 2. 方式决策（先问三个问题）

```
① 这个控件能用现有控件（panel/text/icon/image/button/input/…）拼出来吗？
     能 → 方式一 组合已有控件（DSL 模板 + SpawnElementViewUnderId），只写挂载代码，不写控件类
     不能 ↓
② 只是长相/状态不同，结构行为都够用吗？
     是 → 方式三 只改外观：@style + class + :hover/:focus/:disabled，不写 C++
     否 ↓
③ 需要自绘像素 / 自算排版 / 接管键盘滚动 → 方式二 自绘控件类型
     = AbstractControl 子类（+ 固有尺寸注册）+ ControlFactory 子类 + 启动期 setInstance
```

方式一与方式二可叠加：自绘控件的内部骨架照样能用 DSL 片段。

---

## 3. API 契约速查

### 3.1 必须实现（纯虚）

```cpp
std::string getTypeName() const override;                  // 忽略大小写 == DSL 标签名
void onRender(DisplayList& dl, const ui::Rect& clip) const override;   // 只画
```

### 3.2 可覆盖钩子（按需，别全抄）

| 签名 | 时机 / 用途 |
|---|---|
| `void OnSyncFromResolvedNode(const ResolvedNode& rn)` | 通用映射后同步派生属性；⛔ 初值只应用一次（`initialized_` 守卫） |
| `void onLayoutSynced()` | bounds/viewport/scroll 同步完成 → 重建自己的布局缓存；**必须**先查脏标记 |
| `void onFrameCommit()` | update 之后 paint 之前 → 补建本帧产生的内部脏布局 |
| `void update()` | 按时间推进；需要继续 tick → `requestAnimationTick()` |
| `bool handleLocalMouseButton(const ControlLocalMouseButtonEvent&)` | 局部坐标 + `hitTestLocal`；拖拽用 `captureMouse()` / `releaseMouse()` |
| `bool handleLocalMouseMove(const ControlLocalMouseMoveEvent&)` | 悬停/拖选 |
| `bool handleLocalScroll(const ControlLocalScrollEvent&)` | 滚轮 |
| `bool onKey(const KeyEvent&)` / `bool onChar(const CharEvent&)` | 有焦点时；`true` = 已处理 |
| `void onFocus()` / `void onBlur()` / `void onHover(bool)` / `void onActive(bool)` | 状态回调 |
| `void setScrollSize(float, float)` | 自管滚动时覆盖成空实现（自己算） |
| `bool isContainer() const` | 返回 `true` 才把子节点建成子控件 |

### 3.3 可用工具（基类给的能力）

```cpp
// 外观（已由通用映射同步，直接取用）
getFillColor() / getStrokeColor() / getStrokeWidth() / getCornerRadius() / getPadding*() / isVisible() / bounds_
// 状态
hasClass(c) / addClass(c, forceLayout=false) / removeClass(c, forceLayout=false) / setId / getUid
// 滚动
setScroll / getScrollX|Y / setScrollSize / getScrollWidth|Height / setViewportSize / getViewportWidth|Height / hasViewport
// 请求处理
markLayerDirty()            // 只脏本控件
invalidate()                // 脏 + 请求重绘 + 下帧 tick
requestAnimationTick()      // 只续 tick，不重绘
captureMouse() / releaseMouse() / hitTestLocal(point) / toLocalPoint(point)
getScene()->setCursor(Scene::Cursor_Hand|Arrow|IBeam)
```

### 3.4 其余入口

```cpp
// 固有尺寸（只填 AUTO 的那些维度）
loppe::ContentMeasureRegistry::instance().registerType("mytag", strategy);
//   strategy: void(ResolvedNode*, const Context& ctx, float& w, float& h, float availableWidth)
//   ctx.textMeasurer 可测文字宽高；if (w == AUTO_SENTINEL) w = ...;

// 组合式挂载（返回值就是成败）
ElementView              ComponentInstantiator::SpawnElementViewUnderId (Scene*, parentId, dsl|AstSnippet, ctx, index=-1, ResolvedRuleScope=Local);
std::vector<ElementView> ComponentInstantiator::SpawnElementViewsUnderId(Scene*, parentId, dsl|AstSnippet, ctx, index=-1, ResolvedRuleScope=Local);
// ElementView: valid()/control()/id()/uid()/addClass/removeClass/hasClass/remove()/ancestorByClass/descendantByClass

// 组件生命周期壳
class X : public UIComponent { void onAttach(Scene*, const std::string& parentId) override; };

// 新标签注册
class MyFactory : public ControlFactory {
    std::shared_ptr<AbstractControl> doMakeControlFromResolvedNodeHook(ResolvedNode* rn) override;   // 不认就 return nullptr
    std::shared_ptr<ScrollbarControl> CreateScrollbar(Orientation) override;                        // 可选
};
ControlFactory::setInstance(std::make_unique<MyFactory>());   // 启动期一次

// 控件 → 宿主事件
UIEventSystem::UIEvent e; e.type = UIEventType::Change; e.target = this; e.text = ...;
getScene()->getUIEventSystem()->dispatch(e);
// 宿主监听：scene->onClick("#id"|".class", cb) / onInput / onChange / onScroll
```

---

## 4. 四条口径（写完必查）

| 口径 | 规则 |
|---|---|
| **尺寸** | 显式宽高由样式决定；`auto` 走 `ContentMeasureRegistry`；不改样式就别每帧改尺寸 |
| **滚动** | 自管滚动的控件：`setScrollSize` 空实现 + `AbstractControl::setScrollSize(自算值)` + `setViewportSize(内容区)` + 越界钳位 |
| **重绘** | 外观变了 → `markLayerDirty()`；要通知宿主/继续动画 → `invalidate()` 或 `requestAnimationTick()`；**禁止**每帧 `invalidate()` |
| **事件** | 在 `update()` 里**比较后**才派发（去重）；处理器沿命中路径从深到浅，`stopPropagation` 止传播 |

---

## 5. 代码骨架

### 5.1 组合式（不写控件类）

```cpp
static const loppe::AstSnippet kTpl = []{ loppe::LoppeParser p; return p.parse(R"ui(
@style  { .card { background-color:#1f2733; border-radius:8px; padding:12px; } }
@layout { .card { display:flex; flex-direction:column; } }
panel(class="card") { text(class="t", value="Title"), text(class="v", value="0") }
)ui"); }();

loppe::ElementView v = loppe::ComponentInstantiator::SpawnElementViewUnderId(scene, parentId, kTpl, scene->getDocumentRuntime());
if (!v) return;
```

### 5.2 自绘控件

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
    void  rebuild()    { /* 你的内部布局缓存 */ layoutDirty_ = false; }
};
```

### 5.3 工厂 + 安装

```cpp
class MyFactory : public loppe::ControlFactory {
public:
    std::shared_ptr<loppe::AbstractControl> doMakeControlFromResolvedNodeHook(loppe::ResolvedNode* rn) override {
        if (rn && rn->astNode && rn->astNode->type == "mytag") return std::make_shared<MyControl>();
        return nullptr;   // 交回引擎内置类型
    }
};
// 宿主 Application::build() 里：
loppe::ControlFactory::setInstance(std::make_unique<MyFactory>());
```

---

## 6. 提示词模板（复制即用）

### 模板 1：组合式组件

```
我在用 LOPPE UI（loppe-ui，引擎以预编译包 + 公开头文件提供）做宿主应用。
请实现一个「<一句话描述，例如：带标题和数值的统计卡片>」组件，用组合式方案：
- 一段 DSL 模板（自带 @style/@layout 局部规则），用 AstSnippet::parse 只解析一次，模板写成 id-less；
- 用 ComponentInstantiator::SpawnElementViewUnderId 挂到指定 parentId 下，返回 ElementView；
- 提供 setData(...) 更新显示：优先「复用元素 + 切 class / 改文本」，不要删光重建；
- 状态用 class 表达（如 .active），交互态用 :hover/:focus/:disabled；
- 只输出 .h/.cpp 两个文件 + 需要加进 build.lpj sources 的文件名清单。
只用引擎公开头文件里已有的 API，不要臆造不存在的接口。
```

### 模板 2：自绘控件类型

```
我在用 LOPPE UI（loppe-ui，引擎以预编译包 + 公开头文件提供）做宿主应用。
请实现一个新的 DSL 控件类型 `<tag>`（C++ 类 <Name>），要求：<功能描述>。
必须遵守：
1) 继承 loppe::AbstractControl，实现 onRender(DisplayList&, const ui::Rect& clip) 与
   getTypeName()（返回 "<tag>"，忽略大小写等于 DSL 标签名）；
2) 构造函数里用 ContentMeasureRegistry::registerType("<tag>", strategy) 注册固有尺寸，
   只填 == AUTO_SENTINEL 的维度；
3) 在 OnSyncFromResolvedNode(rn) 里同步派生属性（rn.astNode->attrs 读 DSL 属性、
   rn.getFinalStyle()/rn.props() 读样式），初始化值只应用一次（用 initialized_ 守卫）；
4) 通用外观（背景/边框/圆角/padding/可见性）直接用基类 getter，不要自己解析；
5) 在 onLayoutSynced() 里重建内部布局，必须先查脏标记（禁止无条件重建）；
6) 状态变化用 markLayerDirty()；要通知宿主用 update() 里去重后 UIEventSystem::dispatch
   （宿主 scene->onClick/onInput/onChange 监听）；
7) 同时给出 ControlFactory 子类（doMakeControlFromResolvedNodeHook 认 "<tag>"，其余 return nullptr），
   以及启动期 ControlFactory::setInstance 的接入点；
8) DSL 里这样用：<tag>(id="x", ...)。
输出：控件 .h/.cpp、工厂 .h、接入代码片段、build.lpj sources 增量。
```

### 模板 3：只改外观 / 造变体

```
这是现有 LOPPE UI 控件/组件：<控件名或 .lui 片段>。
不要写 C++，只用 DSL 与 class 改造成 <目标外观/状态>：
- 允许：@style/@layout 规则的增改、class 的增删与组合、:hover/:active/:focus/:disabled 伪状态、
  已有 SVG 图标表里的图标；
- 不允许：新增 C++ 控件类型、使用不存在的属性/选择器、内联硬编码尺寸（优先 flex/百分比）；
- 输出完整可替换的 .lui 片段 + 宿主侧需要 toggle 的 class 名清单；
- 注意引擎差异：overflow 默认 auto（不是 visible）、border-box、无 flex-basis、
  选择器不支持后代/子代组合器。
```

---

## 7. 反面清单（AI 常犯）

| ❌ 错误写法 | ✅ 正确做法 |
|---|---|
| 想在引擎实现里加个类型分支 | 自己工程里的 `ControlFactory` 子类 + `setInstance` |
| `getTypeName()` 返回 C++ 类名 | 返回 DSL 标签名（小写） |
| 自定义标签没配工厂 | 配工厂，或在 DSL 里改用现有标签组合 |
| 工厂装得太晚（控件树已建好） | 启动期 `Application::build()` 里装 |
| 固有尺寸无条件覆盖 `requestedW/H` | 只在 `== AUTO_SENTINEL` 时填 |
| `onLayoutSynced()` 里无条件重建排版 | 先查 `layoutDirty_` |
| 每帧 `invalidate()` 驱动动画 | `requestAnimationTick()`，只在真变了才 `invalidate()` |
| 只 `markLayerDirty()` 却期待宿主收到滚动通知 | 同时把它排进下一帧 tick（`invalidate()`） |
| `onRender` 里标脏 / 重建布局 | 在事件或 `update()` / `onFrameCommit()` 里做 |
| 模板里写死 `id`，然后挂多次 | id-less 模板，或调用方注入唯一 id |
| 列表刷新「删光重建」 | 复用元素 + 换数据/切 class |
| 自绘内容溢出自己的盒子 | `pushClip` 到自己的视口（引擎文本叶子不裁自身盒子） |
| 后台线程直接改控件 | `UiDispatcher::post` 回 UI 线程 |
| 新文件忘了加进 `build.lpj sources` | 同步 sources 与 include 目录 |

---

## 8. 交付自检清单

- [ ] 只用公开头文件里的 API；新代码全在自己的工程里
- [ ] `getTypeName()` == DSL 标签名；工厂 hook 认得这个 type；`setInstance` 在启动期
- [ ] 固有尺寸只填 `AUTO_SENTINEL`
- [ ] `OnSyncFromResolvedNode` 里初值有 `initialized_` 守卫；派生属性变化置脏
- [ ] `onLayoutSynced()` 查脏标记；重活不在渲染期做
- [ ] 重绘用 `markLayerDirty()`/`invalidate()`；按时间推进用 `requestAnimationTick()`
- [ ] 事件在 `update()` 里去重派发；宿主用 `scene->on*("#id"|".class", cb)` 监听
- [ ] 自绘内容 `pushClip` 到自己视口；`onRender` 不标脏
- [ ] 新文件已进 `build.lpj sources`；新目录已进 include
- [ ] 能在 `.lui` 里用新标签，并至少有一条无头用例/最小界面验证过

---

## 9. 速查

**`UIEventType`**：`Click` `MouseDown` `MouseUp` `Input` `Change` `Scroll` `Focus` `Blur` `KeyDown` `KeyUp`

**选择器**（`bind` / `scene->onXxx`）：`#id`、`.class`；样式选择器另有 `type`、`:hover/:active/:focus/:disabled`，
**不支持**后代/子代组合器。

**引擎内置标签**：`panel` `text` `button` `input` `combobox` `radio` `progress` `icon` `image` `scrollbar`（自动管理）

**相关文档**

| 文档 | 用途 |
|---|---|
| `../LOPPE_CUSTOM_CONTROL_GUIDE.md` | 自定义控件完整指南（含完整示例与坑表） |
| `../LOPPE_DSL_USER_GUIDE.md` | DSL 写界面 |
| `LOPPE_DSL_AI_REFERENCE.md` | 属性全表 + 与标准 CSS 的关键差异（同目录） |
| `../LAYOUT_DESIGN.md` | 尺寸模型 / 盒模型 / 滚动容器 |
