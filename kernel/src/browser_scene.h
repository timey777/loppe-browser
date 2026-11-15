#pragma once
// browser_scene.h —— 浏览器外壳（标题栏 / 标签页 / 地址栏 / 状态栏 / 权限条）+ 各个页面。
//
// 用法：
//     // 壳建场景时（桌面 = Application::build，安卓/ESP32 = 各自 runtime 的初始化）
//     auto scene = std::make_shared<loppe_browser::BrowserScene>(runtime);
//     scene->setInitialAddress("loppe");      // 可选：启动就打开这个地址（不设 = 开始页，不联网）
//     scene->setHost(this);                   // 壳实现 browser_host.h：渲染资源 + Stage 挂载
//     Stage::getInstance().replaceScene(scene);
//
// 想改外壳外观改 ui/main.lui；页面长什么样由页面自己的文档决定（app.js 里的 ui.render）；
// 想改地址栏规则改 net/http_url.h 的说明；想加权限能力改 perm/permissions.h 的 Capability。
//
// 多标签：一个标签 = 一个独立的 Stage + 自己的文档 + 自己的 JS 上下文。
// 隔离靠「树根本不是同一棵」（每标签一份 DocumentRuntime），页面里的 id 与别的标签天然
// 无关，不需要任何前缀。切标签 = 换哪个标签的 Stage 接收键盘（Stage::setActive）；
// 关标签 = 整个 Stage 连同文档、控件、JS 上下文一起析构。
// 壳与所有标签的 Stage 都登记在同一个 ViewSlotManager 里（BrowserHost::views()），
// 渲染入口与文字引擎全窗口共用一份（BrowserHost::renderer() / textRenderer()）。
// 后台标签的 JS 照常存活，只是 JS 泵降到 1Hz（见 .cpp 顶部「为什么后台标签要降频」）。

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "loppe.h"

#include "components/stage.h"
#include "engine/pipeline/astbuild_engine.h"   // 全局类（不在 loppe:: 里）
#include "engine/pipeline/box_model_engine.h"
#include "engine/compositor.h"
#include "engine/document_runtime.h"
#include "engine/pipeline/layout_engine.h"
#include "engine/pipeline/style_engine.h"
#include "engine/view_coordinator.h"
#include "misc/throttle.h"

#include "net/http_url.h"
#include "browser_host.h"
#include "page/page_session.h"
#include "perm/permissions.h"

namespace loppe_browser {

class BrowserScene : public loppe::Scene {
public:
    explicit BrowserScene(DocumentRuntime& runtime);
    ~BrowserScene() override;

    bool setup() override;

    // 启动地址（setup 之前设；留空 = 启动开一个开始页标签，不联网）
    void setInitialAddress(const std::string& text) { initialAddress_ = text; }
    // 宿主（setup 之前设）：各标签的 Stage 要登记进它的视图槽表（见 browser_host.h）
    void setHost(BrowserHost* host) { host_ = host; }
    // 配置目录（setup 之前设；缺省空 = 内核自己在 exe 目录向上探测 config/）。
    // exe 落点不止一种布局的壳（如桌面从解决方案根或子工程构建），把探到的绝对目录传进来。
    void setConfigDir(const std::string& dir) { configDir_ = dir; }
    // "config/window.cfg" 这类相对路径 → 实际文件路径（与内核自己读 hosts/permissions
    // 完全同一套探测规则，壳读自己的配置也走这里，别在外面再写一份）。
    std::string ResolveConfigPath(const char* relative) const;

    // 渲染循环每帧调一次：推进所有标签的页面、刷新界面、同步窗口拖动区与页面矩形。
    void tick();

    // 回车 / F5 / Ctrl+T 之类由 Application 转发进来。返回 true = 这个键被浏览器吃掉了。
    bool handleKey(int key, int mods);

    // 地址栏回车（main.lui 里「打开」按钮与 Enter 都走这里）
    void submitAddress();

    // 收起壳与所有标签页里的键盘焦点。点在没有界面元素的标题栏区域时由壳调用
    // （那种点击被系统当 HTCAPTION 吃掉，界面收不到，焦点得由宿主主动收）。
    void clearAllFocus();

    void goBack();
    void goForward();
    void reloadPage();

    // ── 标签 ──
    void newTab();                      // 开一个空白标签页并激活
    void closeTab(int index);           // 关掉第 index 个（关的是最后一个就关窗口）
    void activateTab(int index);
    // 本地文件（拖进窗口的那些路径）：一个文件开一个标签。只登记，真正开标签在下一帧 tick。
    // 支持 .js（当页面跑）与 .lui（直接当文档看）；别的扩展名只提示、不开标签。
    void openLocalFiles(const std::vector<std::string>& paths);
    int tabCount() const { return static_cast<int>(tabs_.size()); }
    int activeIndex() const { return active_; }

    // 还有标签在抓页面（渲染循环用它决定空闲时睡多久）
    bool isLoading() const;

    const std::string& startupError() const { return startupError_; }

private:
    // 事件回调里要做的事（见 .cpp 顶部「为什么事件回调里不直接改界面」）
    struct PendingAction {
        enum class Kind {
            None,
            NewTab,      // 开一个空白标签页
            ActivateTab, // 切到 tab
            CloseTab,    // 关掉 tab
            Navigate,    // 在 tab（-1 = 当前标签）里打开 address（空 = 读地址栏）
            Back,
            Forward,
            Reload,
            PermAllow,
            PermDeny,
            WinMin,
            WinMax,
            WinClose,
        };
        Kind kind = Kind::None;
        int tab = -1;
        std::string address;      // Navigate：地址栏写法（空 = 读地址栏当前值）
        Address target;           // Navigate：已经解析好的地址（targetValid 时优先）
        bool targetValid = false;
    };

    // 一个标签的整套界面：Stage + 文档运行时 + 四件套引擎 + 场景 + 合成/协调器。
    // 渲染入口不在这里：所有标签共用壳的窗口级 Renderer（BrowserHost::renderer()）。
    // 一次导航未来 = replaceScene；现在先复用同一份 scene（PageSession 自己清根容器）。
    struct TabStage {
        loppe::Stage stage;
        ::StyleEngine style;
        ::BoxModelEngine boxModel;
        ::LayoutEngine layout;
        ::ASTBuildEngine astBuild;
        ::DocumentRuntime runtime;
        std::unique_ptr<loppe::Compositor> compositor;
        std::unique_ptr<loppe::ViewCoordinator> host;
        std::shared_ptr<loppe::Scene> scene;
    };

    // 一个标签页。壳里的标签条元素 id 由 serial 派生，关掉不回收（避免旧回调误命中新控件）。
    struct Tab {
        int serial = 0;
        std::string tabId;      // "tab-3"（壳文档：标签条上的一条）
        std::string titleId;    // "tabtitle-3"
        std::string spinId;     // "tabspin-3"：加载转圈（非加载时藏起来）
        std::string favId;      // "tabfav-3"：默认图标（地球）；有远端图标/加载中时藏起来
        std::string imgId;      // "tabimg-3"：远端 favicon（image 控件，抓到才有东西可画）
        // 远端图标的落盘路径；空 = 用默认图标。换页时清掉（新站点的图标要重新抓）。
        std::string iconPath;
        std::unique_ptr<TabStage> ui;   // 页面那一整套（Stage/文档/场景/合成）
        std::unique_ptr<PageSession> page;
        PermissionStore perms;  // 每个标签一份：站点权限跟着页面走，不跟着窗口走
        std::vector<std::string> history;
        size_t historyPos = 0;
        std::string address;    // 地址栏里显示的东西；空 = 新标签页
        std::string title;
        // JS 泵的分频闸门：前台 8ms、后台 1s（间隔由 activateTabNow 设）
        loppe::Throttle pumpGate{std::chrono::milliseconds(8)};
    };

    void bindChrome();

    void openTab(const std::string& address);      // 造 Tab（壳标签条元素 + 页面 Stage）
    void closeTabNow(int index);                   // 真的摘控件/析构 Stage（只从 runPendingAction 调）
    void activateTabNow(int index);                // 真的切键盘焦点（只从 runPendingAction 调）
    int indexOfTabId(const std::string& id) const;
    int indexOfSerial(int serial) const;
    Tab* activeTab();

    void navigateTab(Tab& tab, const std::string& input, bool recordHistory);
    void applyNavigation(Tab& tab, const Address& address, bool recordHistory);
    void goBackNow();
    void goForwardNow();
    void reloadNow();
    void applyModeFromUi();

    void refreshChrome();          // 地址栏 / 状态栏 / 权限条
    void syncAddressBar();         // 地址栏跟着当前标签走（用户正在输入时不动）
    void refreshStatus();
    void refreshPermissionBar();
    void refreshTabTitle(Tab& tab, const std::string& title);
    void refreshTabTitles();       // 每帧核对：首帧写文本时控件可能还没同步好
    void refreshTabActiveState();
    void refreshTabIcons();        // 三选一：加载转圈 / 远端 favicon / 默认地球图标
    void refreshWindowButtons();   // 最大化/还原按钮的图形跟着窗口状态换
    void updateDragRegions();
    void syncPageRects();          // 页面矩形 → Stage::setViewRect / host->setViewport

    void spawnStartPage(Tab& tab); // 新标签页里的「开始页」（挂在页面文档里）
    void openPendingLocalFiles();  // 把 openLocalFiles 攒下的路径真的开成标签（只从 tick 调）

    // 事件回调（点击 / 按键）里只登记意图，真正动手统一放到下一帧 tick 的开头。
    void queue(PendingAction::Kind kind, int tab = -1, std::string address = std::string());
    // 已经解析好的地址（页面里 ui.navigate 走这条：相对地址已按当前页展开）
    void queueAddress(int tab, const Address& address);
    void runPendingAction();

    std::shared_ptr<loppe::AbstractControl> findControl(const std::string& id);
    void setControlText(const std::string& id, const std::string& text);
    void setControlValue(const std::string& id, const std::string& value);
    void setControlClass(const std::string& id, const char* cls, bool on);
    void setControlClassVisible(const std::string& id, bool visible);
    // icon 控件的图形名（"" = 不画，但布局位置仍占着）
    void setControlIconName(const std::string& id, const std::string& name);
    // image 控件的图片（绝对路径或资源相对路径；先 preload 再换，否则这一帧画不出来）
    void setControlImageSrc(const std::string& id, const std::string& src);
    std::string inputValue(const std::string& id);

    BrowserHost* host_ = nullptr;     // 渲染资源 + 视图槽表；壳建场景时注入
    std::string configDir_;           // hosts/permissions 所在目录（绝对；空 = 默认探测）
    std::string permissionsPath_;
    HostMap hostMap_;
    std::vector<std::unique_ptr<Tab>> tabs_;
    int active_ = -1;
    int nextSerial_ = 1;

    AddressMode mode_ = AddressMode::Auto;
    std::string initialAddress_;   // 空 = 启动就开一个开始页标签（不联网）
    std::string startupError_;
    std::string lastStatus_;
    std::vector<std::string> pendingLocalFiles_;   // 拖进来待开的本地文件（tick 里消费）

    PendingAction pending_;
};

} // namespace loppe_browser
