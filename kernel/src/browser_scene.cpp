// ===== 设计说明 =====
//
// 【为什么一个标签一个 Stage，而不是「一个场景里划格子」】引擎的多 Stage（components/stage.h）
// 让一个窗口装多份文档：每个标签一套 Stage + DocumentRuntime + 四件套引擎 + 场景。
// 隔离靠「树根本不是同一棵」——页面里的 id 与别的标签天然无关，不需要 id 前缀/作用域校验；
// 一份文档从哪来：页面脚本 ui.render(lui) 交出来（开始页/错误页由宿主内置同样格式的文本），
// 一次导航 = 整份文档换掉 + JS 上下文换掉（见 page/page_session.cpp）。
// 切标签不销毁：后台标签的定时器、Promise、滚动位置全保留，切回来还是「原来那页」。
//
// 【什么在标签之间共用】窗口级的东西只有一份：drawer / 文字引擎 / Renderer（图标纹理缓存
// 跟着它）/ present 标志 / 视图槽表（ViewSlotManager）。每标签独有的是「一块画布」——
// Stage + Compositor（持有本标签的 DisplayList）+ ViewCoordinator（绑本标签的 runtime）。
// 所以 attach 一个标签只是往槽表里加一条，不新建渲染后端。
//
// 【为什么壳收不到页面的事件】onClick/ui.on 绑在各自 scene 的事件系统上 —— 标签条在壳
// 文档里、页面内容在页面文档里；同一个控件的事件不会跨 scene 冒泡，谁绑谁收。
//
// 【为什么容器 id / 标签 id 只增不回收】壳里的事件是挂在场景上的，关掉的标签留下的回调还在。
// 如果 id 复用（比如关了 3 号再开一个又叫 3 号），旧回调会命中新控件——点一下关掉别人。
// 序号只增，旧 id 永远指不到东西，回调自然失效。（页面文档随 Stage 一起析构，没有这个问题。）
//
// 【为什么状态栏/权限条只在变化时写控件】setText / addClass 都会标脏并触发一次重绘。
// 如果每帧无脑刷一遍，浏览器在空闲时也在满帧重画——笔记本风扇会告诉你。
// 所有「刷界面」的入口都先比较缓存值，变了才写。
//
// 【为什么地址栏记的是用户输入，不是最终 IP】用户回看历史时应该看到 `loppe`，
// 而不是 `127.0.0.1:8080`。真实去向放在状态栏里（origin + 是否本地映射），
// 这样「这个别名到底连到哪」一眼可见——本地映射最容易在这里出意外。
//
// 【为什么解析失败不关当前页】和浏览器一致：地址错了只报错，页面留着。
// 关掉当前页会让用户丢掉正在看的东西。
//
// 【为什么事件回调里不直接改界面】引擎派发点击/按键时，手里握着一条「命中路径」
// （从最深控件到根的裸指针），事件处理器里如果拆控件（换页要清空页面容器、关标签要摘元素），
// 路径上的节点就被销毁了，冒泡的下一跳直接踩空 → 崩溃。所以处理器只登记
// pending_（开标签/切标签/关标签/导航/窗口按钮），真正动手在下一帧 tick 的开头一次性做完；
// 那时没有派发在跑，改树是安全的。代价是点击到生效差一帧（≤8ms），肉眼看不出来。
// 页面里的 ui.navigate 走同一条通道（理由一样，只是调用者从按钮变成了脚本）。
//
// 【为什么后台标签要降频】一个后台标签的 JS 每帧照跑，等于给每个开着的页面都留一份
// 常驻定时器：QuickJS 空转 + 页面里 setTimeout 的回调全部按 deadline 触发，切到后台
// 什么也看不见却一直在烧 CPU。降频分两档就够：前台 8ms（跟手，和人眼无关，和事件密度有关），
// 后台 1s（够定时器/时钟类页面不"停住"，又不至于每帧唤醒）。分频用引擎的 loppe::Throttle
// （misc/throttle.h）——「距上次成功过了 N 毫秒」这一件事不应该每个宿主自己写一遍。
// 抓取结果的交接（poll）不降频：那是一次锁 + 一个 bool，而加载中的后台标签必须能加载完。

#include "browser_scene.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include "loppe.h"

#include "components/component_instantiator.h"
#include "components/control_factory.h"
#include "components/container_control.h"
#include "components/element_view.h"
#include "components/icon.h"
#include "components/image.h"
#include "components/input.h"
#include "components/text.h"
#include "engine/core/ast_snippet.h"
#include "engine/core/parse.h"
#include "engine/core/resolved_node.h"
#include "engine/core/resolved_node_query.h"
#include "engine/core/resolved_tree_view.h"
#include "engine/event/ui_event_system.h"
#include "engine/resource/resource_manager.h"

#include "page/script_scene.h"
#include "window_host.h"

namespace loppe_browser {
namespace {

// 界面文件的落点也有两种（与 desktop/src/main.cpp 的 FindConfigDir 同源）：
//   desktop/ 里单独构建 → exe 在 desktop/build/<配置>/，上两级就是 desktop/；
//   解决方案根构建     → exe 在 <根>/build/<配置>/，界面仍在 <根>/desktop/ui/。
// 所以除了「本级 ui/」，每层还要试一次「本级 desktop/ui/」。
constexpr const char* kUiCandidates[] = { "ui/main.lui",
                                          "../ui/main.lui",
                                          "../../ui/main.lui",
                                          "../../desktop/ui/main.lui",
                                          "../../../desktop/ui/main.lui" };

// 页面文档的底：新标签、换页中间态都用它。页面自己的文档由脚本 ui.render 交出来。
constexpr const char* kBlankPageLui =
    "@style {\n"
    "    .page { width: 100%; height: 100%; background-color: #ffffff; }\n"
    "}\n"
    "@layout {\n"
    "    .page { display: flex; flex-direction: column; }\n"
    "}\n"
    "panel(class=\"page\") {}\n";

// JS 泵的分频档位（见文件顶部「为什么后台标签要降频」）
constexpr std::chrono::milliseconds kForegroundPump{ 8 };
constexpr std::chrono::seconds kBackgroundPump{ 1 };

// 标签标题前转圈的节奏：8 帧 × 80ms = 一圈 640ms。图标名由外壳注册（desktop/src/shell_icons.cpp），
// 别的壳没注册时这里照常切名，只是画不出图形（不报错）。
constexpr int kSpinnerFrames = 8;
constexpr long long kSpinnerFrameMs = 80;

// 收起某个 Stage 里那份界面的键盘焦点。键盘换了归属时用（切标签 / 点进页面 / 点窗口标题栏）：
// 引擎的 updateFocus 只在「被点中的那份文档」里清焦点，别的 Stage 里那个输入框会一直挂着
// Focus 状态 —— 外发光不灭、光标停在最后一次闪烁的样子，而且它不再在前台 tick，看着就是卡的。
void ClearFocusOf(loppe::Stage* stage)
{
    if (!stage) return;
    auto scene = stage->currentScene();
    if (!scene) return;
    if (auto* ui = scene->getUIEventSystem()) ui->updateFocus(nullptr);
}

const char* ModeLabel(AddressMode m)
{
    switch (m) {
        case AddressMode::Domain: return "域名";
        case AddressMode::Local: return "本地映射";
        case AddressMode::Auto: break;
    }
    return "自动";
}

AddressMode ModeFromLabel(const std::string& label)
{
    if (label == "域名") return AddressMode::Domain;
    if (label == "本地映射") return AddressMode::Local;
    return AddressMode::Auto;
}

// 事件目标 → 往上找第一个带这个 class 的控件，取它的 id。
// 走裸指针不走 ElementView::fromControl：后者是按 id 反查的，控件没写 id 时会查到别人身上。
std::string AncestorIdWithClass(loppe::AbstractControl* ctrl, const char* cls)
{
    for (loppe::AbstractControl* cur = ctrl; cur; cur = cur->getParent()) {
        if (cur->hasClass(cls)) return cur->getId();
    }
    return std::string();
}

std::string Trim(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// 拖进来的本地文件能不能当页面：.js 当脚本跑，.lui 直接当文档看（见 PageSession::poll）。
// 别的类型（图片 / html / 二进制）不支持，宿主只说一句「打不开」，不开空标签。
bool IsLocalPageFile(const std::string& path)
{
    const size_t dot = path.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".js" || ext == ".mjs" || ext == ".lui";
}

bool ReadFileIfExists(const std::string& path, std::string* out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    *out = ss.str();
    return !out->empty();
}

// 开始页（新标签页）就是一份普通文档：页面脚本产的是它，宿主内置的也是它。
// 它**不联网、也不列别名**：纯静态一段文字，任何环境下打开都长一样。
std::string BuildStartPageLui()
{
    return
        "@style {\n"
        "    .page { width: 100%; height: 100%; background-color: #ffffff; padding: 20px; overflow: scroll; }\n"
        "    .start-title { font-size: 24px; color: #101828; }\n"
        "    .start-tip { font-size: 13px; color: #5b6b85; }\n"
        "    .start-hint { font-size: 12px; color: #8a94a6; }\n"
        "}\n"
        "@layout {\n"
        "    .page { display: flex; flex-direction: column; gap: 10px; }\n"
        "}\n"
        "panel(class=\"page\") {\n"
        "    text(class=\"start-title\", value=\"新标签页\"),\n"
        "    text(class=\"start-tip\", value=\"在地址栏输入地址回车打开：不写端口就是 80，不写路径就是 /app.js。\"),\n"
        "    text(class=\"start-tip\", value=\"把 .js / .lui 文件拖进窗口，会在新标签里打开它（file:///… 也能在地址栏直接输入）。\"),\n"
        "    text(class=\"start-tip\", value=\"Ctrl+T 新标签页 · Ctrl+W 关标签页 · Ctrl+Tab 切标签 · F5 重新加载\"),\n"
        "    text(class=\"start-hint\", value=\"本地演示站点：python -m http.server 8080 --directory examples，然后打开 127.0.0.1:8080\")\n"
        "}\n";
}


// 标签页上的文字：用 host[:port]，够短也能认出是谁；本地文件用文件名
std::string SiteTitle(const Address& a)
{
    if (a.scheme == Scheme::File) {
        const size_t cut = a.path.rfind('/');
        const std::string name = cut == std::string::npos ? a.path : a.path.substr(cut + 1);
        return name.empty() ? std::string("本地文件") : name;
    }
    if (a.host.empty()) return "新标签页";
    // 本地映射的别名本身就是站点名，端口是内部细节，不往标签上写
    if (a.fromAlias) return a.host;
    std::string t = a.host;
    if (a.port != kDefaultPort) t += ":" + std::to_string(a.port);
    return t;
}

// 标签条按列宽截断（中文算两列）——Text 没有 text-overflow，只能自己截
std::string ClipText(const std::string& s, size_t maxCols)
{
    size_t cols = 0;
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = 1;
        size_t w = 1;
        if (c >= 0xF0) { len = 4; w = 2; }
        else if (c >= 0xE0) { len = 3; w = 2; }
        else if (c >= 0xC0) { len = 2; w = 2; }
        if (cols + w > maxCols) return s.substr(0, i) + "…";
        cols += w;
        i += len;
    }
    return s;
}

// 页面场景：一份 .lui 文档 = 一个标签的全部界面（Stage::pushScene 会调 setup）。
// 文档由页面脚本产出（ui.render），这里的 initialLui 只是首帧的底（空白页 / 开始页）。
class PageScene : public ScriptScene {
public:
    PageScene(DocumentRuntime& runtime, std::string initialLui)
        : ScriptScene(runtime), initialLui_(std::move(initialLui)) {}

    bool setup() override
    {
        if (initialLui_.empty()) {
            LOPPE_ERROR("[page] 初始文档是空的");
            return false;
        }
        try {
            loppe::LoppeParser parser;
            auto ast = parser.parse(initialLui_).root();
            if (!ast) {
                LOPPE_ERROR("[page] 初始文档解析后是空的");
                return false;
            }
            getDocumentRuntime()->notifyASTUpdated(ast);
            getDocumentRuntime()->runPipelineRenderTreeOnly(loppe::ViewportSizeDp{ { 1100 }, { 700 } });
            auto* tree = getResolvedTree();
            if (tree && !tree->getRoot()) {
                auto* docTree = getDocumentRuntime()->getResolvedTree();
                if (docTree && docTree->getRoot()) {
                    tree->attachSubtreeFromOther(*docTree, nullptr, false);
                }
            }
        } catch (const std::exception& ex) {
            LOPPE_ERROR(std::string("[page] 初始文档解析失败：") + ex.what());
            return false;
        }
        return true;
    }

private:
    std::string initialLui_;
};

} // namespace

BrowserScene::BrowserScene(DocumentRuntime& runtime) : Scene(runtime) {}

BrowserScene::~BrowserScene() = default;

std::string BrowserScene::ResolveConfigPath(const char* relative) const
{
    // relative 形如 "config/hosts.cfg"。壳通过 setConfigDir 给了绝对目录就直接用。
    if (!configDir_.empty()) {
        std::string file(relative);
        const std::size_t slash = file.find('/');
        return configDir_ + "/" + (slash == std::string::npos ? file : file.substr(slash + 1));
    }
    // 默认：exe 目录向上探测（config/ 与 exe 工程同根的布局）
    const std::string candidates[] = {
        FileSystem::getPath(relative),
        FileSystem::getPath(std::string("../") + relative),
        FileSystem::getPath(std::string("../../") + relative),
        FileSystem::getPath(std::string("../../../") + relative),
    };
    for (const std::string& p : candidates) {
        std::ifstream probe(p, std::ios::binary);
        if (probe) return p;
    }
    return candidates[0];
}

bool BrowserScene::setup()
{
    std::string source;
    std::string uiPath;
    for (const char* rel : kUiCandidates) {
        const std::string path = FileSystem::getPath(rel);
        if (ReadFileIfExists(path, &source)) {
            uiPath = path;
            break;
        }
    }
    if (uiPath.empty()) {
        startupError_ = "找不到 ui/main.lui（exe 旁边、向上两级、以及各层的 desktop/ui 都找过了）";
        LOPPE_ERROR(startupError_);
        return false;
    }

    ResourceManager::getInstance().setDocumentRoot(
        std::filesystem::path(uiPath).parent_path().string());

    try {
        loppe::LoppeParser parser;
        auto ast = parser.parse(source).root();
        if (!ast) {
            startupError_ = "ui/main.lui 解析后是空的";
            LOPPE_ERROR(startupError_);
            return false;
        }
        getDocumentRuntime()->notifyASTUpdated(ast);
        getDocumentRuntime()->runPipelineRenderTreeOnly(loppe::ViewportSizeDp{ { 1100 }, { 760 } });
        auto* tree = getResolvedTree();
        if (tree && !tree->getRoot()) {
            auto* docTree = getDocumentRuntime()->getResolvedTree();
            if (docTree && docTree->getRoot()) {
                tree->attachSubtreeFromOther(*docTree, nullptr, false);
            }
        }
    } catch (const std::exception& ex) {
        startupError_ = std::string("ui/main.lui 解析失败：") + ex.what();
        LOPPE_ERROR(startupError_);
        return false;
    }

    hostMap_.load(ResolveConfigPath("config/hosts.cfg"));
    permissionsPath_ = ResolveConfigPath("config/permissions.cfg");

    bindChrome();
    if (auto* sched = scheduler()) sched->requestRepaint();   // 壳文档自己的调度器

    openTab(initialAddress_);
    return true;
}

void BrowserScene::bindChrome()
{
    onClick("#nav-go", [this](loppe::UIEventSystem::UIEvent&) {
        queue(PendingAction::Kind::Navigate);          // 地址读执行时的值
    });
    onClick("#nav-back", [this](loppe::UIEventSystem::UIEvent&) {
        queue(PendingAction::Kind::Back);
    });
    onClick("#nav-forward", [this](loppe::UIEventSystem::UIEvent&) {
        queue(PendingAction::Kind::Forward);
    });
    onClick("#nav-reload", [this](loppe::UIEventSystem::UIEvent&) {
        queue(PendingAction::Kind::Reload);
    });

    onClick("#tab-new", [this](loppe::UIEventSystem::UIEvent&) {
        queue(PendingAction::Kind::NewTab);
    });
    onClick("#win-min", [this](loppe::UIEventSystem::UIEvent&) {
        queue(PendingAction::Kind::WinMin);
    });
    onClick("#win-max", [this](loppe::UIEventSystem::UIEvent&) {
        queue(PendingAction::Kind::WinMax);
    });
    onClick("#win-close", [this](loppe::UIEventSystem::UIEvent&) {
        queue(PendingAction::Kind::WinClose);
    });

    // 标签条是运行期造出来的，只能按 class 绑；点标签里的任何东西都会冒泡到这里
    onClick(".tab", [this](loppe::UIEventSystem::UIEvent& e) {
        const int i = indexOfTabId(AncestorIdWithClass(e.target, "tab"));
        if (i >= 0) queue(PendingAction::Kind::ActivateTab, i);
    });
    // 关标签：必须先停住冒泡，否则同一次点击接着会「激活」这个正在被关掉的标签
    onClick(".tab-close", [this](loppe::UIEventSystem::UIEvent& e) {
        e.stopPropagation = true;
        const int i = indexOfTabId(AncestorIdWithClass(e.target, "tab"));
        if (i >= 0) queue(PendingAction::Kind::CloseTab, i);
    });
    // 开始页链接：绑在每个页面 scene 自己的事件系统上（PageScene::setup），不在这里

    onClick("#perm-allow", [this](loppe::UIEventSystem::UIEvent&) {
        queue(PendingAction::Kind::PermAllow);
    });
    onClick("#perm-deny", [this](loppe::UIEventSystem::UIEvent&) {
        queue(PendingAction::Kind::PermDeny);
    });

    // 组合框换模式只改一个枚举 + 一段文字，不碰控件树，就地做掉
    onChange("#mode", [this](loppe::UIEventSystem::UIEvent& e) {
        mode_ = ModeFromLabel(e.text);
        lastStatus_ = std::string("解析模式 → ") + ModeLabel(mode_);
        refreshStatus();
    });
}

BrowserScene::Tab* BrowserScene::activeTab()
{
    if (active_ < 0 || active_ >= static_cast<int>(tabs_.size())) return nullptr;
    return tabs_[active_].get();
}

int BrowserScene::indexOfTabId(const std::string& id) const
{
    if (id.empty()) return -1;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (tabs_[i]->tabId == id) return static_cast<int>(i);
    }
    return -1;
}

int BrowserScene::indexOfSerial(int serial) const
{
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (tabs_[i]->serial == serial) return static_cast<int>(i);
    }
    return -1;
}

void BrowserScene::openTab(const std::string& address)
{
    auto tab = std::make_unique<Tab>();
    tab->serial = nextSerial_++;
    tab->tabId = "tab-" + std::to_string(tab->serial);
    tab->titleId = "tabtitle-" + std::to_string(tab->serial);
    tab->spinId = "tabspin-" + std::to_string(tab->serial);
    tab->favId = "tabfav-" + std::to_string(tab->serial);
    tab->imgId = "tabimg-" + std::to_string(tab->serial);
    tab->title = "新标签页";

    auto* runtime = getDocumentRuntime();

    // ① 壳：标签条上的一条
    // class 里直接带上 tab-active：openTab 结尾一定会激活这个新标签，而**选中态必须在
    // 首次建树时就带上** —— 控件刚 spawn 时控件树还没同步完，那一刻 addClass 只会改控件
    // 自己的类表、样式不重算，之后每帧的 refreshTabActiveState 又被 hasClass 挡掉，
    // 结果就是标签一直显示成未选中的颜色（而不是继续追加重算，就不该在这时候加类）。
    // 标题前那 14×14 的一格是三选一（图标的可见性由 refreshTabIcons 每帧定）：
    //   加载中 → 转圈；抓到远端 favicon → image 控件；其余 → 默认地球图标。
    // 三个都占同一格、同一尺寸，所以「换成哪一个」不会让标题左右跳。
    // 转圈图故意不写 name：Icon 只在 name 属性非空时才 setName（见 icon.cpp），
    // 写死了就没法在运行期换成别的帧。
    const std::string tabDsl =
        "panel(id=\"" + tab->tabId + "\", class=\"tab tab-active\") {"
        "icon(id=\"" + tab->favId + "\", name=\"shell-tab-globe\", class=\"tab-icon\"),"
        "image(id=\"" + tab->imgId + "\", class=\"tab-icon hidden\"),"
        "icon(id=\"" + tab->spinId + "\", class=\"tab-icon hidden\"),"
        "text(id=\"" + tab->titleId + "\", class=\"tab-title\", value=\"新标签页\"),"
        "button(class=\"tab-close\") { icon(name=\"shell-close\", class=\"tab-close-icon\") }"
        "}";
    loppe::LoppeParser parser;
    const loppe::AstSnippet tabSnippet = parser.parse(tabDsl);
    if (!loppe::ComponentInstantiator::SpawnElementViewUnderId(this, "tabs", tabSnippet, runtime)) {
        LOPPE_WARN("[tab] 标签条元素挂不上（ui/main.lui 里少了 #tabs？）");
    }

    // ② 页面：独立的 Stage + 文档树 + 场景（见文件顶部「为什么一个标签一个 Stage」）
    tab->ui = std::make_unique<TabStage>();
    TabStage& ts = *tab->ui;
    ts.runtime.registerService(&ts.style);
    ts.runtime.registerService(&ts.layout);
    ts.runtime.registerService(&ts.astBuild);
    ts.runtime.registerService(&ts.boxModel);
    ts.runtime.initializeAll({ typeid(::StyleEngine), typeid(::LayoutEngine),
                               typeid(::ASTBuildEngine), typeid(::BoxModelEngine) });
    if (host_) {
        if (auto* tr = host_->textRenderer()) ts.runtime.setTextMeasurer(tr);
    }
    ts.compositor = std::make_unique<loppe::Compositor>();
    // 渲染入口是窗口级的那一个（图标纹理缓存、drawer 指针全标签共用一份）
    if (host_) ts.compositor->setRenderer(host_->renderer());

    auto scene = std::make_shared<PageScene>(ts.runtime, kBlankPageLui);
    ts.scene = scene;

    std::weak_ptr<loppe::Scene> weak = ts.scene;
    ts.host = std::make_unique<loppe::ViewCoordinator>(
        &ts.runtime,
        loppe::ViewportSizeDp{ { 1100 }, { 700 } },
        [weak]() -> loppe::Scene* { return weak.lock().get(); });
    ts.host->bind(scene.get());
    ts.stage.replaceScene(scene);
    // 平台能力（光标 / 剪贴板 / 文件选择）全窗口共用一份：Scene::getPlatformHost() 与
    // flushCursor() 都是经 Stage::active() 取宿主的，标签 Stage 不接上就等于「激活后没有宿主」
    // —— 现象是鼠标移到地址栏不再变 I 形、剪贴板也失灵。
    ts.stage.setPlatformHost(loppe::Stage::getInstance().getPlatformHost());
    if (host_) host_->views().attach(&ts.stage, ts.compositor.get(), ts.host.get());

    tab->perms.load(permissionsPath_);
    // 这里的 scene 一定是 PageScene（ScriptScene 派生）；PageSession 只面向页面场景
    tab->page = std::make_unique<PageSession>(static_cast<ScriptScene*>(ts.scene.get()),
                                             &ts.runtime, ts.host.get(), &tab->perms, &hostMap_);

    // 页面里的 ui.navigate：这里解析（地址栏模式 + 映射表 + 当前页作基准），
    // 然后排队到下一帧 —— 不能在脚本回调里就地换页（见文件顶部）。
    const int serial = tab->serial;
    tab->page->setNavigateHandler([this, serial](const std::string& url) -> std::string {
        const int index = indexOfSerial(serial);
        if (index < 0) return "标签已经关掉了";

        const std::string ref = Trim(url);
        if (ref.empty()) return "url 是空的";

        ResolveOutcome r;
        if (ref[0] == '/' || ref.rfind("./", 0) == 0 || ref.rfind("../", 0) == 0) {
            // 页面内相对地址：以当前页为基准（换页不会换站点）
            r = ResolveLocation(tabs_[index]->page->address(), ref);
        } else {
            // 完整地址：和地址栏同一套规则（别名 / 域名 / 本地映射都认）
            hostMap_.load(ResolveConfigPath("config/hosts.cfg"));
            r = ParseAddress(ref, mode_, hostMap_);
        }
        if (!r.ok) return r.error;

        queueAddress(index, r.address);
        return std::string();
    });

    tabs_.push_back(std::move(tab));
    const int index = static_cast<int>(tabs_.size()) - 1;
    Tab& fresh = *tabs_[index];

    if (address.empty()) spawnStartPage(fresh);
    else navigateTab(fresh, address, true);

    activateTabNow(index);
}

// 开始页（页面文档里的控件）：纯静态一份文档，不联网、不带可点元素
void BrowserScene::spawnStartPage(Tab& tab)
{
    if (!tab.page) return;
    tab.page->showDocument(BuildStartPageLui(), "就绪");
}

void BrowserScene::newTab()
{
    queue(PendingAction::Kind::NewTab);
}

// 拖进窗口的文件（壳的 drop 回调）：只登记，开标签留到下一 frame 的 tick ——
// 回调可能发生在引擎的事件派发里，那儿不能造 Stage、不能换文档（见文件顶部）。
void BrowserScene::openLocalFiles(const std::vector<std::string>& paths)
{
    pendingLocalFiles_.insert(pendingLocalFiles_.end(), paths.begin(), paths.end());
}

void BrowserScene::openPendingLocalFiles()
{
    if (pendingLocalFiles_.empty()) return;
    std::vector<std::string> batch;
    batch.swap(pendingLocalFiles_);

    for (const std::string& path : batch) {
        ResolveOutcome parsed = ParseLocalFile(path);
        if (!parsed.ok) {
            lastStatus_ = parsed.error;
            refreshStatus();
            continue;
        }
        if (!IsLocalPageFile(parsed.address.path)) {
            lastStatus_ = "只能打开 .js / .lui 文件：" + path;
            refreshStatus();
            continue;
        }
        openTab(parsed.address.url());     // 每个文件一个标签，最后一个留在前台
    }
}

void BrowserScene::closeTab(int index)
{
    queue(PendingAction::Kind::CloseTab, index);
}

void BrowserScene::activateTab(int index)
{
    queue(PendingAction::Kind::ActivateTab, index);
}

// 壳文档里 spawn 出来的元素（标签条条目）怎么退役：
// 按 id 从 resolved 树查节点 → RetireResolvedNode(eraseFromAst)。
// 不用 RemoveResolvedNodeByUid：uid 表是进程级全局的，ResolvedTree::clear() 会把它整张清掉
// （见引擎 resolved_tree.cpp），而浏览器每个标签都有自己的树 —— 任何一次文档重建之后
// 按 uid 删除就静默失效，控件留在父容器里继续占位、继续画。
void RetireShellElement(loppe::Scene* scene, const std::string& id)
{
    if (!scene || id.empty()) return;
    auto* tree = scene->getResolvedTree();
    if (!tree) return;
    loppe::ResolvedNode* node = loppe::ResolvedNodeQuery::FindById(tree, id);
    if (!node) return;
    loppe::ControlFactory::getInstance()->RetireResolvedNode(node, /*eraseFromAst=*/true);
}

void BrowserScene::closeTabNow(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;

    // 最后一个标签关掉 = 关窗口（和桌面浏览器一致，不把一个空壳留在屏幕上）
    if (tabs_.size() == 1) {
        windowHost().Close();
        return;
    }

    Tab& t = *tabs_[index];
    if (t.page) t.page->close();        // 先断 JS 回调，再拆东西
    t.page.reset();                     // JS 上下文随页面文档一起走
    if (t.ui && host_) host_->views().detach(&t.ui->stage);   // 先从渲染/输入里摘掉
    t.ui.reset();                       // Stage + 文档 + 控件 + 场景整体析构
    RetireShellElement(this, t.tabId);

    tabs_.erase(tabs_.begin() + index);

    if (active_ > index) --active_;
    else if (active_ == index) active_ = std::min(index, static_cast<int>(tabs_.size()) - 1);

    activateTabNow(active_);
}

void BrowserScene::activateTabNow(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;
    active_ = index;

    loppe::Stage* const previousStage = loppe::Stage::active();
    // 切前台/后台 = 改这个标签的泵频。切回前台时 reset 一下：等满 8ms 再泵，
    // 免得刚切回来就补跑一整个后台周期的定时器。
    for (size_t i = 0; i < tabs_.size(); ++i) {
        const bool isActive = static_cast<int>(i) == active_;
        tabs_[i]->pumpGate.setMinInterval(isActive ? kForegroundPump : kBackgroundPump);
        if (isActive) tabs_[i]->pumpGate.reset();
        // 键盘焦点与可见性都跟活动标签走（点地址栏等壳区域时由 BrowserApp 切回壳）
        if (tabs_[i]->ui) tabs_[i]->ui->stage.setVisible(isActive);
        if (isActive && tabs_[i]->ui) loppe::Stage::setActive(&tabs_[i]->ui->stage);
        else if (!isActive && tabs_[i]->ui && loppe::Stage::active() == &tabs_[i]->ui->stage) {
            loppe::Stage::setActive(nullptr);
        }
    }

    // 换了归属就把上一份界面的焦点收掉（切标签时地址栏可能还亮着外发光、光标冻着）
    if (previousStage != nullptr && previousStage != loppe::Stage::active()) {
        ClearFocusOf(previousStage);
    }

    // 切回前台时同步一次页面矩形（窗口在后台期间可能变过尺寸）
    syncPageRects();

    lastStatus_.clear();
    refreshTabActiveState();
    refreshChrome();
    if (auto* sched = scheduler()) sched->requestRepaint();   // 壳文档自己的调度器
}

void BrowserScene::refreshTabActiveState()
{
    for (size_t i = 0; i < tabs_.size(); ++i) {
        const bool on = static_cast<int>(i) == active_;
        setControlClass(tabs_[i]->tabId, "tab-active", on);
    }
}

// 标题前那一格：三种图形三选一。所有标签共用一个转圈相位（一起转看着整齐）。
// 为什么每帧都算一遍：`page->takeIconPath()` 是「抓好了就给我」，不知道哪一帧会有；
// setControlIconName / setControlImageSrc / setControlClassVisible 都自带 diff，没变不标脏。
void BrowserScene::refreshTabIcons()
{
    static const auto epoch = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - epoch).count();
    const std::string spinning =
        "shell-spin-" + std::to_string((elapsed / kSpinnerFrameMs) % kSpinnerFrames);

    for (auto& tab : tabs_) {
        if (!tab->ui) continue;

        // 图标是后到的：到了就记在 Tab 上，换页时才清
        if (tab->page) {
            const std::string icon = tab->page->takeIconPath();
            if (!icon.empty()) {
                tab->iconPath = icon;
                setControlImageSrc(tab->imgId, icon);
            }
        }

        const bool loading = tab->page && tab->page->loading();
        const bool hasIcon = !tab->iconPath.empty();

        if (loading) setControlIconName(tab->spinId, spinning);
        setControlClassVisible(tab->spinId, loading);
        setControlClassVisible(tab->imgId, !loading && hasIcon);
        setControlClassVisible(tab->favId, !loading && !hasIcon);
    }
}

// 最大化/还原按钮的图形随窗口状态换：双击标题栏、系统菜单、Win+↑ 都能改这个状态，
// 不是只有点我们那个按钮才会变，所以每帧按 windowHost().IsMaximized() 对一次，
// 不是在哪条动作里顺手改一下（setControlIconName 自带「没变不标脏」，每帧对的代价是零）。
void BrowserScene::refreshWindowButtons()
{
    setControlIconName("win-max-icon", windowHost().IsMaximized() ? "shell-restore" : "shell-max");
}

void BrowserScene::submitAddress()
{
    queue(PendingAction::Kind::Navigate);
}

// 点在没有界面元素的标题栏上：窗口还是激活的，但界面里的键盘焦点该收
// （原生窗口点标题栏也是这个效果）。
void BrowserScene::clearAllFocus()
{
    if (auto* ui = getUIEventSystem()) ui->updateFocus(nullptr);
    for (auto& tab : tabs_) {
        if (tab && tab->ui) ClearFocusOf(&tab->ui->stage);
    }
}

void BrowserScene::navigateTab(Tab& tab, const std::string& input, bool recordHistory)
{
    hostMap_.load(ResolveConfigPath("config/hosts.cfg"));   // 改映射表不用重启

    ResolveOutcome r = ParseAddress(input, mode_, hostMap_);
    if (!r.ok) {
        lastStatus_ = r.error;
        refreshStatus();
        return;
    }
    applyNavigation(tab, r.address, recordHistory);
}

// 地址已经解析好的那条路（页面里的 ui.navigate）：不能再走一次 ParseAddress，
// 否则相对地址的来源（别名 host / connectHost）会被地址栏模式重新解释一遍。
void BrowserScene::applyNavigation(Tab& tab, const Address& address, bool recordHistory)
{
    if (!tab.page) return;

    lastStatus_.clear();
    tab.address = address.url();
    if (recordHistory) {
        // 截掉「位置之后」的前进记录再追加。这里是 historyPos + 1，不是 historyPos：
        // resize(n) 保留前 n 条，用 historyPos 会把当前这一条也砍掉（只有一条记录时
        // 直接砍成空 → 后退永远报「已经是最早的一页」）。
        tab.history.resize(tab.historyPos + 1);
        tab.history.push_back(tab.address);   // 存完整地址：回退时重新解析不会丢端口/路径
        tab.historyPos = tab.history.size() - 1;
    }
    tab.page->open(address);
    // 换了站点就得重新抓图标：上一页的 favicon 连同路径一起作废，先退回默认地球图标
    //（PageSession::open 里也会扔掉在飞的图标线程，两边各管自己那一半）。
    tab.iconPath.clear();
    setControlImageSrc(tab.imgId, std::string());
    refreshTabTitle(tab, SiteTitle(address));

    if (&tab == activeTab()) {
        setControlValue("addr", tab.address);
        refreshStatus();
    }
}

void BrowserScene::goBack()
{
    queue(PendingAction::Kind::Back);
}

void BrowserScene::goBackNow()
{
    Tab* t = activeTab();
    if (!t) return;
    if (t->history.empty() || t->historyPos == 0) {
        lastStatus_ = "已经是最早的一页";
        refreshStatus();
        return;
    }
    --t->historyPos;
    navigateTab(*t, t->history[t->historyPos], false);
}

void BrowserScene::goForward()
{
    queue(PendingAction::Kind::Forward);
}

void BrowserScene::goForwardNow()
{
    Tab* t = activeTab();
    if (!t) return;
    if (t->history.empty() || t->historyPos + 1 >= t->history.size()) {
        lastStatus_ = "已经是最后一页";
        refreshStatus();
        return;
    }
    ++t->historyPos;
    navigateTab(*t, t->history[t->historyPos], false);
}

void BrowserScene::reloadPage()
{
    queue(PendingAction::Kind::Reload);
}

void BrowserScene::reloadNow()
{
    Tab* t = activeTab();
    if (!t || !t->page) return;
    lastStatus_.clear();
    t->page->reload();
    refreshStatus();
}

bool BrowserScene::handleKey(int key, int mods)
{
    const bool ctrl = (mods & 0x0002) != 0;     // GLFW_MOD_CONTROL
    const bool shift = (mods & 0x0001) != 0;    // GLFW_MOD_SHIFT

    if (ctrl && key == 84) {                    // Ctrl+T
        queue(PendingAction::Kind::NewTab);
        return true;
    }
    if (ctrl && key == 87) {                    // Ctrl+W
        queue(PendingAction::Kind::CloseTab, active_);
        return true;
    }
    if (ctrl && key == 258) {                   // Ctrl+Tab / Ctrl+Shift+Tab
        const int n = tabCount();
        if (n > 1) queue(PendingAction::Kind::ActivateTab, (active_ + (shift ? n - 1 : 1)) % n);
        return true;
    }

    switch (key) {
        case 257:   // Enter
        case 335:   // 小键盘 Enter
            queue(PendingAction::Kind::Navigate);
            return true;
        case 294:   // F5
            queue(PendingAction::Kind::Reload);
            return true;
        default:
            break;
    }
    return false;
}

bool BrowserScene::isLoading() const
{
    for (const auto& t : tabs_) {
        if (t->page && t->page->loading()) return true;
    }
    return false;
}

void BrowserScene::tick()
{
    if (pending_.kind != PendingAction::Kind::None) runPendingAction();
    openPendingLocalFiles();            // 拖进来的文件：开成新标签（可能把空窗口填满，别放到 isEmpty 之后）
    if (tabs_.empty()) return;

    for (size_t i = 0; i < tabs_.size(); ++i) {
        Tab& t = *tabs_[i];
        if (!t.page) continue;
        // 抓取交接每帧都做：后台标签也得能加载完（代价只是一次锁 + 一个 bool）。
        t.page->poll();
        // JS 泵按标签分频：前台 8ms、后台 1s。
        if (t.pumpGate.tryAcquire()) t.page->pumpJs();
    }

    refreshPermissionBar();
    // 状态栏、标签标题、标签选中态每帧校验：文本自带 diff，类表变化会去重，
    // 没变不会标脏；而切标签、开新标签、页面跳转会让它们变，那些时刻不一定伴随页面事件。
    // 尤其是「刚 spawn 出来的标签条元素」——那一刻控件树还没同步完，一次性调用会打空。
    refreshStatus();
    refreshTabTitles();
    refreshTabActiveState();
    refreshTabIcons();
    refreshWindowButtons();
    syncAddressBar();
    syncPageRects();

    // 标题栏是自己画的，系统不知道哪块能拖窗口——每帧把矩形报过去
    updateDragRegions();
}

// 页面矩形（壳里 #page 的位置）→ 各标签 Stage 的 viewRect，再由视图槽表下发视口。
// 引擎的鼠标路由与叠画都按 viewRect 走（见 components/stage.h），矩形变了必须每帧对齐；
// 视口判等与 handleResize 归槽表（applySlotResize），这里不再自己记「上次的矩形」。
void BrowserScene::syncPageRects()
{
    if (!host_) return;
    auto region = findControl("page");
    if (!region) return;   // 首帧壳控件树还没同步完，下一 tick 再来
    const auto r = region->screenBounds();
    const loppe::ui::Rect rect(r.x, r.y, r.width, r.height);

    for (size_t i = 0; i < tabs_.size(); ++i) {
        Tab& t = *tabs_[i];
        if (!t.ui || !t.ui->host) continue;
        // 后台标签不跟随尺寸（切回来时再同步）；从没同步过的必须先给一个矩形。
        if (static_cast<int>(i) != active_ && t.ui->stage.hasViewRect()) continue;

        t.ui->stage.setViewRect(rect);
        host_->views().applySlotResize(&t.ui->stage);
    }
}

void BrowserScene::queue(PendingAction::Kind kind, int tab, std::string address)
{
    pending_ = PendingAction();      // 清干净：target/targetValid 不能从上一次残留下来
    pending_.kind = kind;
    pending_.tab = tab;
    pending_.address = std::move(address);
}

void BrowserScene::queueAddress(int tab, const Address& address)
{
    pending_ = PendingAction();
    pending_.kind = PendingAction::Kind::Navigate;
    pending_.tab = tab;
    pending_.target = address;
    pending_.targetValid = true;
}

void BrowserScene::runPendingAction()
{
    const PendingAction action = pending_;
    pending_ = PendingAction();

    switch (action.kind) {
        case PendingAction::Kind::None:
            return;

        case PendingAction::Kind::NewTab:
            openTab(std::string());
            return;

        case PendingAction::Kind::ActivateTab:
            activateTabNow(action.tab);
            return;

        case PendingAction::Kind::CloseTab:
            closeTabNow(action.tab);
            return;

        case PendingAction::Kind::Navigate: {
            const int index = action.tab >= 0 ? action.tab : active_;
            if (index < 0 || index >= static_cast<int>(tabs_.size())) return;

            // 页面自己发起的跳转（ui.navigate）不抢前台：后台标签里的跳转就该留在后台。
            if (action.targetValid) {
                applyNavigation(*tabs_[index], action.target, true);
                return;
            }

            if (index != active_) activateTabNow(index);

            std::string input = action.address;
            if (input.empty()) input = inputValue("addr");       // 「打开」按钮 / 回车：读地址栏
            if (input.empty()) {
                lastStatus_ = "地址栏是空的";
                refreshStatus();
                return;
            }
            navigateTab(*tabs_[index], input, true);
            return;
        }

        case PendingAction::Kind::Back:
            goBackNow();
            return;

        case PendingAction::Kind::Forward:
            goForwardNow();
            return;

        case PendingAction::Kind::Reload:
            reloadNow();
            return;

        case PendingAction::Kind::PermAllow:
        case PendingAction::Kind::PermDeny: {
            const bool allow = action.kind == PendingAction::Kind::PermAllow;
            if (Tab* t = activeTab(); t && t->page) t->page->resolvePermission(allow);
            refreshPermissionBar();
            refreshStatus();
            return;
        }

        case PendingAction::Kind::WinMin:
            windowHost().Minimize();
            return;

        case PendingAction::Kind::WinMax:
            windowHost().ToggleMaximize();
            return;

        case PendingAction::Kind::WinClose:
            windowHost().Close();
            return;
    }
}

void BrowserScene::syncAddressBar()
{
    Tab* t = activeTab();
    if (!t) return;
    auto ctrl = findControl("addr");
    auto* in = dynamic_cast<loppe::Input*>(ctrl.get());
    if (!in || in->isFocused()) return;          // 用户正在输入：别抢
    if (in->getValue() != t->address) in->setValue(t->address, false);
}

void BrowserScene::updateDragRegions()
{
    auto bar = findControl("titlebar");
    if (!bar) return;
    const auto barRect = bar->screenBounds();
    DipRect strip{ barRect.x, barRect.y, barRect.width, barRect.height };

    std::vector<DipRect> noDrag;
    auto add = [&noDrag](const std::shared_ptr<loppe::AbstractControl>& c) {
        if (!c) return;
        const auto r = c->screenBounds();
        if (r.isEmpty()) return;
        noDrag.push_back(DipRect{ r.x, r.y, r.width, r.height });
    };
    for (const auto& tab : tabs_) add(findControl(tab->tabId));
    add(findControl("tab-new"));
    add(findControl("win-min"));
    add(findControl("win-max"));
    add(findControl("win-close"));

    windowHost().SetDragRegions(strip, noDrag);
}

void BrowserScene::refreshChrome()
{
    Tab* t = activeTab();
    setControlValue("addr", t ? t->address : std::string());
    refreshStatus();
    refreshPermissionBar();
}

void BrowserScene::refreshStatus()
{
    Tab* t = activeTab();

    // 页面还没建好（刚开始页标签）也要写状态栏，否则会留着上一个标签的文本。
    std::string text = (t && t->page && lastStatus_.empty()) ? t->page->status() : lastStatus_;
    if (text.empty()) text = "就绪";
    text += "   ·   ";
    text += ModeLabel(mode_);
    if (tabs_.size() > 1) {
        text += "   ·   ";
        text += std::to_string(tabs_.size()) + " 个标签";
    }

    if (t && t->page) {
        const Address& addr = t->page->address();
        if (!addr.host.empty()) {
            text += "   ·   ";
            text += addr.origin();
            text += addr.fromAlias ? "（本地映射）" : "（域名）";
        }
    }

    setControlText("status", text);
}

void BrowserScene::refreshPermissionBar()
{
    Tab* t = activeTab();
    const PageSession::PermissionRequest req =
        (t && t->page) ? t->page->pendingPermission() : PageSession::PermissionRequest();

    setControlClassVisible("perm", req.pending);
    if (req.pending) {
        setControlText("perm-text", req.origin + " 想用「" + CapabilityName(req.capability) + "」");
    }
}

void BrowserScene::refreshTabTitle(Tab& tab, const std::string& title)
{
    tab.title = title.empty() ? "新标签页" : title;
    setControlText(tab.titleId, ClipText(tab.title, 18));
}

// 标签条标题每帧核对一次（setControlText 自带 diff，没变不标脏）。
// 导航到「标签条元素刚 spawn 出来」的时刻写文本会打空——那时控件树还没同步完，
// 一次性的 refreshTabTitle 丢了就再也补不回来，标签会一直挂着默认标题。
void BrowserScene::refreshTabTitles()
{
    for (const auto& tab : tabs_) {
        if (!tab) continue;
        setControlText(tab->titleId, ClipText(tab->title, 18));
    }
}

std::shared_ptr<loppe::AbstractControl> BrowserScene::findControl(const std::string& id)
{
    auto* cm = getControlManager();
    if (!cm || id.empty()) return nullptr;
    return cm->findControlById(id);
}

void BrowserScene::setControlText(const std::string& id, const std::string& text)
{
    auto ctrl = findControl(id);
    auto* t = dynamic_cast<loppe::Text*>(ctrl.get());
    if (!t) return;
    if (t->getText() == text) return;   // 只在真的变了才标脏
    t->setText(text);
}

void BrowserScene::setControlValue(const std::string& id, const std::string& value)
{
    auto ctrl = findControl(id);
    auto* in = dynamic_cast<loppe::Input*>(ctrl.get());
    if (!in) return;
    if (in->getValue() == value) return;
    in->setValue(value, false);         // false = 不触发 change，免得把「回填」当成用户输入
}

void BrowserScene::setControlClass(const std::string& id, const char* cls, bool on)
{
    auto ctrl = findControl(id);
    if (!ctrl) return;
    const bool has = ctrl->hasClass(cls);
    if (on && !has) ctrl->addClass(cls);
    else if (!on && has) ctrl->removeClass(cls);
}

void BrowserScene::setControlClassVisible(const std::string& id, bool visible)
{
    setControlClass(id, "hidden", !visible);
}

void BrowserScene::setControlIconName(const std::string& id, const std::string& name)
{
    auto ctrl = findControl(id);
    auto* icon = dynamic_cast<loppe::Icon*>(ctrl.get());
    if (!icon) return;
    if (icon->getName() == name) return;   // 没变不标脏：转圈每帧都会走到这里
    icon->setName(name);
    icon->invalidateBounds();              // setName 自己不清脏，光改名字不会重绘
}

// 图片是异步加载的：先 preloadImage（丢给资源线程解码），再换 src，画面在纹理就绪后自己出来
// —— 中间那几帧 getImage 拿不到纹理，渲染器会跳过绘制，不会画花。
void BrowserScene::setControlImageSrc(const std::string& id, const std::string& src)
{
    auto ctrl = findControl(id);
    auto* img = dynamic_cast<loppe::Image*>(ctrl.get());
    if (!img) return;
    if (img->getSrc() == src) return;
    if (!src.empty()) ResourceManager::getInstance().preloadImage(src);
    img->setSrc(src);
    img->invalidateBounds();
}

std::string BrowserScene::inputValue(const std::string& id)
{
    auto ctrl = findControl(id);
    auto* in = dynamic_cast<loppe::Input*>(ctrl.get());
    return in ? in->getValue() : std::string();
}

} // namespace loppe_browser
