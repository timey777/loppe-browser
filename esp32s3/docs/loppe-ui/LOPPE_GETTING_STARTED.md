# LOPPE 从 C++ 到界面：第一个应用

> 📖 中文 | [English](LOPPE_GETTING_STARTED_EN.md)

---

## 1. 本文讲什么

本文讲如何从一个空白的 C++ 工程开始，做出一个能显示 `.lui` 界面、能响应点击、能在运行期改界面的应用程序。全文按四步推进：搭工程骨架 → 建窗口 → 加载界面 → 让界面动起来。

**适用读者**：会写 C++（能读构建脚本、会用 `std::string` / `std::shared_ptr` / lambda），第一次使用 LOPPE UI 的开发者。不需要了解引擎内部实现。

**前置条件**

| 项 | 要求 |
|---|---|
| 引擎包 | 已拿到 LOPPE UI 的预编译包：`include/`（公开头文件）+ 库文件 |
| 编译器 | 与引擎包一致的工具链（MSVC 或 clang，见包的说明） |
| 图形依赖 | GLFW、GLAD、FreeType（随引擎包提供，或由你的工程提供） |
| DSL 基础 | 不需要，本文给出第一份完整界面；语法细节见《DSL 用户指南》 |

**内容导览**

| 你想做的事 | 去第几节 |
|---|---|
| 弄明白引擎与我的代码怎么分工 | §2 |
| 让窗口跑起来并显示第一段文字 | §3 |
| 把界面写成 `.lui` 文件并从磁盘加载 | §4 |
| 处理点击 / 输入 / 选中变化 | §5 |
| 在 C++ 里读改界面上的控件 | §6 |
| 用自定义字体、图片、SVG 图标 | §7 |
| 编译、运行、发布 | §8 |
| 出了显示或交互问题 | §9 |

---

## 2. 引擎与你的工程各自负责什么

| 谁 | 负责 |
|---|---|
| 引擎 | 解析 `.lui`、计算样式与布局、把控件树画到窗口、命中测试与事件派发、文本测量与滚动 |
| 你的工程 | 建窗口与主循环、决定界面文件从哪来（读文件）、把解析结果交给引擎、订阅事件、在 C++ 里改控件、提供字体 / 图片 / 图标 |

启动与每帧的时序如下。你只需要实现两处：`Application` 子类（建窗口）和 `Scene` 子类（管界面）。

```text
main()
 └ Application::run()
    ├ 建窗口、初始化资源、装载默认字体、注入文本测量器、注册内置图标（chevron-*）
    ├ build(...)                      ← 你在这里创建 Scene 并挂到 Stage
    ├ Scene::setup()                  ← 你在这里加载 .lui、订阅事件（⚠ 这时还没有控件）
    │   └ 解析 .lui → 把结果交给文档运行时（notifyASTUpdated）
    ├ ← 首帧 AST 同步：控件在这里才被创建出来（你在这一步之前 findControlById 一律为空）
    ├ Scene::onControlsReady()        ← 控件建好之后（可能多次触发，需幂等）→ 改控件的活都放这
    └ 每帧循环：样式/布局 → 绘制 → 输入事件 → 你订阅的回调
```

**`setup()` 只做三件事**：读界面文件 → 把解析结果交给文档运行时（`notifyASTUpdated`）→ 用选择器字符串
绑事件（`onClick` / `onChange` / `onInput`，绑定与控件是否已存在无关）。**不要**在 `setup()` 里取控件改属性：
那一刻控件还没建出来，`findControlById()` 静默返回空，写下的 `setOptions` / `setValue` 一个都不会生效。
改控件的初始化统一放 `onControlsReady()`（见 §6「什么时候可以取控件」）。

---

## 3. 如何搭出第一个能跑的工程

### 步骤一：建目录

```text
myapp/
├── build.lpj            构建描述
├── main.cpp             入口 + Application 子类 + Scene 子类
├── ui/
│   └── hello.lui        界面文件（§4 再写，先建空目录）
└── resources/           资源根：字体、图片、图标
```

### 步骤二：写 `build.lpj`

一个可执行程序就是一个 `exe` 目标。`sources`、`include`、`link` 都是显式列表，不会自动收集文件；新增源码要手工加进 `sources`。

```text
exe myapp {
    sources = [ main.cpp ]
    include = [ ., <引擎包>/include ]
    link    = [ loppe, glfw3, glad, freetype ]
    define  = [ LOPPE_STATIC ]
    flags   = [ ${optflags} ]
}
```

| 字段 | 作用 |
|---|---|
| `sources` | 你的 `.cpp` 列表（新增文件要加在这里） |
| `include` | 头文件搜索目录：先是你自己的目录，再加引擎包的 `include` |
| `link` | 引擎库 + 图形依赖；Windows 上还需补 `opengl32`、`gdi32` |
| `define` | `LOPPE_STATIC` 表示静态链接引擎库 |

### 步骤三：写 `main.cpp`

```cpp
#include "application.h"                 // Application
#include "components/stage.h"            // Stage
#include "components/scene.h"            // Scene
#include "engine/document_runtime.h"     // DocumentRuntime

#include <memory>

// 场景：一棵控件树 + 它的交互逻辑。
class HelloScene : public loppe::Scene {
public:
    explicit HelloScene(DocumentRuntime& runtime) : loppe::Scene(runtime) {}

    bool setup() override {
        // §4 在这里加载 .lui；§5 在这里订阅事件。
        return true;
    }
};

// 应用：建窗口、跑主循环。
class HelloApp : public Application {
public:
    HelloApp() : Application("Hello LOPPE", 800.0f, 600.0f) {}

protected:
    // build() 在窗口建好、引擎初始化完成之后调用一次。
    bool build(DocumentRuntime& runtime, loppe::Compositor&, float, float) override {
        loppe::Stage::getInstance().replaceScene(std::make_shared<HelloScene>(runtime));
        return true;   // 返回 false 表示启动失败，run() 会退出
    }
};

int main(int argc, char* argv[]) {
    HelloApp app;
    return app.run(argc, argv);   // 阻塞直到窗口关闭，返回进程退出码
}
```

要点：

- `Application(title, width, height)` 的宽高是**逻辑像素**，窗口会按系统缩放换算。
- `build()` 返回 `true` 才继续；在这里创建场景并交给 `Stage`。
- `Stage::getInstance().replaceScene(scene)` 替换当前场景（旧场景随之析构）；`pushScene` 是压栈，返回时回到上一个场景。
- 需要主动退出（例如命令行自检）时调用 `Application::requestClose()`。

### 步骤四：编译运行

```bash
loppe-cli build . Debug --targets=myapp
```

产物在 `build/Debug/`。直接运行可执行文件，得到一个 800×600 的窗口（此时还没有内容，§4 加上界面）。

---

## 4. 如何把界面写成 `.lui` 文件并从磁盘加载

### 界面文件

界面与 C++ 的分工：界面文件只描述结构与外观，C++ 负责数据与行为。把下面这段存成 `ui/hello.lui`：

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
    text:title(class="title", value="你好，LOPPE"),
    button:btnHello(class="btn") {
        text(value="点我")
    }
}
```

### 加载三步

在 `HelloScene::setup()` 里做三件事：读文件、告诉引擎这份 `.lui` 在哪（`~/` 相对路径的基准）、把解析结果交给文档运行时。

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

    // ① 找到文件：相对路径按资源根（以及可执行文件目录）解析
    const std::string full = res.resolvePath(path);
    std::ifstream in(full, std::ios::binary);
    if (!in) return false;
    std::ostringstream buf;
    buf << in.rdbuf();
    const std::string source = buf.str();

    // ② 记住这份 .lui 的目录：界面里写 `~/img/a.png` 时按它解析
    const auto slash = full.find_last_of("/\\");
    res.setDocumentRoot(slash == std::string::npos ? std::string() : full.substr(0, slash));

    // ③ 解析并交给引擎
    loppe::LoppeParser parser;
    auto ast = parser.parse(source).root();
    if (!ast) return false;
    getDocumentRuntime()->notifyASTUpdated(ast);
    return true;
}
```

`setup()` 改成：

```cpp
bool setup() override {
    return loadDocument("ui/hello.lui");
}
```

要点：

- **解析结果由你的代码持有**（上面的 `ast`）。引擎不读磁盘、也不知道文件从哪来——从网络、内存或压缩包取内容都可以。
- `notifyASTUpdated()` 会走一遍样式 → 布局 → 控件重建，界面随即出现。
- 改完 `.lui` 想立刻生效：重新读文件、重新 `parse`、再调一次 `notifyASTUpdated()` 即可，不必重建窗口。
- 为了让 §6 能把运行期的值写回界面描述，把 `ast` 存成成员变量。

---

## 5. 如何让界面响应操作

`Scene` 提供四个按选择器绑定的便捷方法，选择器支持 `#id` 与 `.class`：

```cpp
bool setup() override {
    if (!loadDocument("ui/hello.lui")) return false;

    onClick("#btnHello", [this](const loppe::UIEventSystem::UIEvent& e) {
        // e.target   = 事件当前到达的控件
        // e.deepestTarget = 实际被点中的最深层控件
        setTitle("按钮被点了");
    });

    onInput("#nameInput", [](const loppe::UIEventSystem::UIEvent&) { /* 文本变了 */ });
    onChange("#cityBox",   [](const loppe::UIEventSystem::UIEvent& e) { /* e.text = 选中项文本 */ });
    onScroll("#list",      [](const loppe::UIEventSystem::UIEvent&) { /* 滚轮 */ });
    return true;
}
```

要监听更细的事件，用事件系统直接绑定，并指定事件类型：

```cpp
getUIEventSystem()->bind(
    "#btnHello",
    loppe::UIEventSystem::UIEventType::MouseDown,
    [](const loppe::UIEventSystem::UIEvent&) { /* 按下 */ });
```

**事件类型**

| 类型 | 何时产生 | 谁派发 |
|---|---|---|
| `Click` | 在同一个控件上按下并抬起 | 所有可命中的控件 |
| `MouseDown` / `MouseUp` | 鼠标键按下 / 抬起 | 所有可命中的控件 |
| `Input` | 文本控件的值发生变化 | `input` |
| `Change` | 控件自身选项/选中项变化 | `combobox` |
| `Scroll` | 滚轮（指针事件 `onScroll` 的另一种用法） | 滚动容器 |
| `Focus` / `Blur` | 获得 / 失去输入焦点 | `input` |
| `KeyDown` / `KeyUp` | 按键按下 / 抬起 | 获得焦点的控件 |

**事件如何到达处理器**

1. 引擎按坐标做命中测试，得到一条从根到最深节点的路径。
2. 事件先给最深节点，再沿祖先链向上冒泡；父容器上的绑定能收到子控件的事件。
3. 处理器里把 `e.stopPropagation = true`，事件不再向上传播，同一节点上排在后面的处理器也不再执行。

事件在 UI 线程派发。后台线程拿到数据后，要跨线程交给引擎，再用 UI 线程的回调去改控件。

---

## 6. 如何在 C++ 里读写界面

### 取控件

按 `id` 取控件，再转成具体控件类型：

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

运行时改的常见接口：

| 控件 | 运行期接口 |
|---|---|
| `text` | `setText` / `getText` / `setFontSize` / `setTextColor` / `setAlignment` / `setWrap` |
| `input` | `setValue` / `getValue` / `setPlaceholder` / `setType` / `selectAll` / `selection` |
| `progress` | `setProgress(0..1)` / `setBarColor` |
| `combobox` | `setOptions`（替换列表）/ `setSelectedIndex` / `getSelectedIndex` / `setPlaceholder` / `setArrowIcon`（展开箭头用的图标名） |
| `radio` | `setChecked` / `isChecked` / `setGroup` |
| `icon` | `setName` |
| `image` | `setSrc` |
| 任意控件 | `setEnabled(false)` 进入禁用态；禁用后不接收悬停、按下与焦点 |

控件接口只允许在 UI 线程调用，与所有控件一致。

### 什么时候可以取控件

控件**不是** `setup()` 建出来的：`notifyASTUpdated()` 只是把 AST 交给文档运行时，真正的控件树要等**第一帧的 AST 同步**（`ViewCoordinator::applyASTRepaint` 里的 `sync(structural)`）才出现，那时 `setup()` 早已返回。所以在 `setup()` 里 `findControlById()` **一律返回空指针**，而且不报错——静默什么都不做（下拉框没有选项、输入框没初值、进度条不动，多半就是这里）。

凡是「要改控件」的初始化都放进 `Scene::onControlsReady()`：控件建好之后、渲染之前回调；一帧内可能被调用多次，主树重建还会换控件实例，所以每次都要试一遍并各带幂等守卫。

```cpp
void onControlsReady() override {
    // 幂等：一帧内可能多次回调 + 界面重解析会换控件
    if (auto control = getControlManager()->findControlById("cityBox"))
        if (auto box = std::dynamic_pointer_cast<loppe::ComboBox>(control))
            if (box->getSelectedIndex() < 0) box->setSelectedIndex(0);
}
```

界面被**重新**解析（例如切换页面）之后，旧控件指针会失效 —— 也在这条回调里重取。

### 把运行期的值写回界面描述

`input` 这类控件的内容是运行期状态，界面描述里的 `value` 不会自动更新。需要保存用户填写内容时：

```cpp
syncToAST(astRoot_, /*overwrite=*/true);   // astRoot_ = §4 解析得到的根节点（Scene 的成员方法）
```

`overwrite=false`（默认）只写回界面描述里还没有 `value` 的节点。

---

## 7. 如何引入字体、图片与图标

### 资源根

相对路径（以及图片 `src`、`icon` 的注册名以外的资源）按下述顺序查找：

1. `~/` 前缀：**相对当前 `.lui` 文件所在目录**（需先 `setDocumentRoot`，见 §4）。
2. 绝对路径且文件存在。
3. 资源根：`setResourceRoot(路径)` 指定的目录，以及它下面的 `resources/`。
4. 可执行文件所在目录，以及它下面的 `resources/`。

在 `build()` 里设置资源根：

```cpp
bool build(DocumentRuntime& runtime, loppe::Compositor&, float, float) override {
    loppe::ResourceManager::getInstance().setResourceRoot("resources");
    loppe::Stage::getInstance().replaceScene(std::make_shared<HelloScene>(runtime));
    return true;
}
```

### 字体

引擎启动时若一个字体都没注册，会尝试加载一个系统默认字体（桌面平台可用，嵌入式平台会失败），所以最简单的工程不必配置字体。要指定字体：

```cpp
auto& res = loppe::ResourceManager::getInstance();
res.loadFont("resources/fonts/NotoSansSC-Regular.ttf", "body");  // 注册名
res.setDefaultFont("body", 14.0f);                               // 默认字体与字号
```

界面里按字体族名使用：`font-family: body;`。**中文字体通常不含 emoji 字形**，需要 emoji 时另配一份带 emoji 的字体并给对应节点单独指定 `font-family`。

### 图片

```loppe
image(class="logo", src="~/img/logo.png")     /* 相对本 .lui 目录 */
image(class="photo", src="pics/a.jpg")        /* 相对资源根 */
```

图片异步加载，加载完成后引擎自行安排一次重绘，宿主不需要轮询。尺寸不给时按图片原始像素参与测量；给了 `width` / `height` 就按给的值绘制。`background-image` 也支持同一套路径规则，但只支持本地文件路径，不支持网络地址。

### 图标

图标是注册到引擎的 SVG 文本，启动时注册一次，界面里按名字使用：

```cpp
loppe::svg::RegisterIcon("close", "<svg viewBox=\"0 0 16 16\">…</svg>");
```

```loppe
icon(class="icon", name="close")
```

图标按当前 `color` 着色，尺寸走 `width` / `height`；因为颜色是样式属性，写 `icon:hover { color: … }` 就会在悬停时换色。

---

## 8. 如何编译、运行与分发

```bash
# 编译（工程当前目录，Debug 配置，目标名取自 build.lpj）
loppe-cli build . Debug --targets=myapp

# 只校验 build.lpj 与 .lui 语法、不产出二进制
loppe-cli config .
```

命令、参数、产物位置与编译输出怎么读，见《用 loppe-cli 构建与运行》（`LOPPE_CLI_GUIDE.md`）。
工程只用 `build.lpj` 描述，没有 `CMakeLists.txt` / `.sln` 这类构建文件。

| 项目 | 说明 |
|---|---|
| 产物 | `build/<配置>/` 下的可执行文件 |
| 运行时需要 | 可执行文件、`resources/`（字体/图片）、界面 `.lui` 文件 |
| 引擎库 | `LOPPE_STATIC` 时静态链接进可执行文件，分发时不需要额外带库 |
| 界面文件位置 | 由你的加载代码决定（§4）。放可执行文件目录或其 `resources/` 下时，无需额外配置资源根 |

---

## 9. 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| 窗口打开但空白 | `build()` 没创建场景，或 `setup()` 没把解析结果交给运行时 | 确认 `replaceScene` 被调用；确认 `setup()` 返回 `true` 且调用了 `notifyASTUpdated()` |
| 界面文件读不到 | 路径既不在资源根、也不在可执行文件目录下 | 打印 `resolvePath(path)` 的返回值，或显式 `setResourceRoot` |
| 界面里 `~/` 图片加载不了 | 没有设置文档目录 | 加载 `.lui` 时调用 `setDocumentRoot(它的目录)` |
| 文字是方块或中文不显示 | 当前字体没有这些字形 | `loadFont` 加载带中文的字体并 `setDefaultFont` |
| 点了没反应 | 绑定的选择器与界面里的 `id` / `class` 不一致；控件被禁用 | 核对 `#id`；禁用状态用 `isEnabled()` 确认；父容器上的绑定依赖冒泡，注意中途是否有 `stopPropagation` |
| 输入框内容取不到 | 运行期值没有回写界面描述 | 保存前调用 `syncToAST(astRoot, true)` |
| 改了 `.lui` 没变化 | 文件只在启动时加载一次 | 重新读文件、重新解析并再次 `notifyASTUpdated()` |
| 取控件返回空指针 | `setup()` 阶段控件还没建出来（首帧 AST 同步之后才有），或界面被重新解析过 | 改控件的代码一律放进 `onControlsReady()`（幂等）里取；界面更新后在那里重取 |
| 下拉框点开像没反应 | 选项列表是空的 → 弹层高度 = 选项数 × 30 = 0 | 宿主用 `setOptions()` 灌数据（`options` 属性只给静态列表）；灌选项的代码多半误放在 `setup()` 里 |
| 下拉框没有右侧箭头 | `arrow` 写了引擎没内置、宿主也没注册的图标名 | 缺省 `chevron-down` 与 `chevron-up`/`right`/`left` 是内置的；换成别的名字要先 `svg::RegisterIcon` 注册 |

---

## 10. 下一步

| 主题 | 文档 |
|---|---|
| `.lui` 语法、选择器、样式与布局属性 | 《DSL 用户指南》 |
| 尺寸模型、flex / grid 的空间分配规则 | 《布局设计》 |
| 每个内置控件有哪些属性与接口 | 《内置控件参考》 |
| 引擎没有的控件怎么自己实现 | 《自定义控件用户指南》 |

---

## 相关文档

| 文档 | 内容 |
|---|---|
| `LOPPE_DSL_USER_GUIDE.md` | DSL 语法、选择器、样式与布局属性、完整示例 |
| `LOPPE_BUILTIN_CONTROLS.md` | 内置控件的属性、默认样式、事件与运行期接口 |
| `LAYOUT_DESIGN.md` | 确定性布局的尺寸模型与空间分配规则 |
| `LOPPE_CUSTOM_CONTROL_GUIDE.md` | 组合已有控件 / 自绘控件类型 / 只改外观 |
| `LOPPE_MULTI_UI.md` | 界面不止一份 `.lui`：外壳 + 挂载点、多界面切换、共享样式 |
| `ai/LOPPE_DSL_AI_REFERENCE.md` | 属性全表 + 与标准 CSS 的关键差异（AI 用） |
