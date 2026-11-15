// ===== 设计说明 =====
//
// 【页面 = 一份 .lui 文档】脚本不逐个造控件：它调用 ui.render(lui) 交出整份文档，
// 宿主解析后整份替换这个标签的文档。样式（@style）也归页面自己，宿主不再有「UA 样式」。
//
// 【为什么抓取要开线程】HttpGet 是同步阻塞的。在主线程里等本地服务是几个毫秒、
// 等一台不在的机器是几秒——那几秒窗口会僵住，用户以为浏览器挂了。抓取丢给工作线程，
// 主线程每帧只做一次「有结果了吗」的交接。
//
// 【为什么不直接 eval 抓回来的字节流】ScriptHost 要求源码 NUL 结尾（quickjs-ng 的词法
// 分析先解引用再判边界），eval(std::string) 会自己拷一份带 NUL 的副本，所以这里统一走
// 字符串重载。
//
// 【为什么收页要先把 JS 关掉再换文档】事件回调是挂在场景上的，里面握着 JS 上下文。
// 先 shutdown（会让残留回调整体失效），再换文档，任何顺序反过来都可能调到已销毁的 ctx。
//
// 【为什么换文档要清掉场景视图的根】解析树是「按身份对齐」的增量更新器，整份文档换掉时
// 旧根节点会被回收；场景视图还指着它就成了悬垂指针。清掉根之后，ViewCoordinator 那一趟
// 会把新根重新 adopt 进来（adoptRootFrom 只在视图无根时生效）。
//
// 【为什么换完文档要立刻跑一趟全量】脚本里常见「render 之后马上 ui.set(...)」：
// 文档解析完就有 ResolvedNode 了，但控件要等控件同步才存在。这一趟把控件建出来，
// 脚本后续的 ui.set 才命中得到东西。
//
// 【权限判定放在 net 回调里，而不是 JS 里】JS 侧只看到「net.get 抛了
// PermissionDenied」；真正的判定、记录、落盘都在宿主。这样页面永远改不掉自己的权限。
//
// 【为什么 ui.navigate 不在回调里直接换页】换页要拆掉当前页的 JS 上下文和整份文档，
// 而调用点几乎一定在事件回调里（引擎正握着那条命中路径）。所以这里只把 url 转给宿主，
// 由宿主排到下一帧再换 —— 和外壳按钮走的是同一条 PendingAction 通道。

#include "page/page_session.h"
#include "page/script_scene.h"

#include <algorithm>
#include <cctype>
#include <exception>

#include "loppe.h"

#include "components/scene.h"
#include "engine/core/ast_node.h"
#include "engine/document_runtime.h"
#include "engine/document_update_plan.h"
#include "engine/view_coordinator.h"

#include "js/script_host.h"
#include "net/favicon.h"
#include "net/resource.h"

namespace loppe_browser {
namespace {

// 路径后缀（大小写不敏感）：本地文件靠它决定「当脚本跑」还是「当文档看」
bool hasSuffix(const std::string& path, const char* suffix)
{
    const size_t n = std::char_traits<char>::length(suffix);
    if (path.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        const unsigned char a = static_cast<unsigned char>(path[path.size() - n + i]);
        const unsigned char b = static_cast<unsigned char>(suffix[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}

// DSL 属性值：这个语言的字符串没有转义机制，引号/换行一律换成单引号与空格
std::string dslEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\n' || c == '\r' || c == '\t') out.push_back(' ');
        else out.push_back(c);
    }
    return out;
}

// 空白页：关闭标签 / 换页中间态的底。必须自带全屏白底，否则会透出窗口底色。
constexpr const char* kBlankLui =
    "@style {\n"
    "    .page { width: 100%; height: 100%; background-color: #ffffff; }\n"
    "}\n"
    "@layout {\n"
    "    .page { display: flex; flex-direction: column; }\n"
    "}\n"
    "panel(class=\"page\") {}\n";

} // namespace

PageSession::PageSession(ScriptScene* scene, DocumentRuntime* runtime,
                         loppe::ViewCoordinator* host, PermissionStore* perms,
                         const HostMap* hostMap)
    : scene_(scene), runtime_(runtime), host_(host), perms_(perms), hostMap_(hostMap) {}

PageSession::~PageSession()
{
    // 析构路径只停 worker、断 JS 回调，不重建文档：此时宿主（TabStage/DocumentRuntime）
    // 马上整体销毁，applyDocument 那一趟 apply 既浪费又危险（实测段错误）。
    // 「清掉页面文档」的语义只留给还活着的宿主显式调 close()（关标签路径）。
    stopWorker();
    js_.reset();
    loading_ = false;
}

void PageSession::setStatus(const std::string& text)
{
    status_ = text;
}

void PageSession::open(const Address& addr)
{
    stopWorker();
    js_.reset();          // 先断回调
    applyDocument(kBlankLui);

    current_ = addr;
    failed_ = false;
    pending_ = PermissionRequest();
    if (perms_) {
        perms_->setOrigin(addr.origin());
        perms_->resetRuntimePrompts();
    }

    setStatus("加载中 " + addr.url());
    loading_ = true;
    startLoad(addr);
}

void PageSession::reload()
{
    const bool empty = current_.scheme == Scheme::File ? current_.path.empty() : current_.host.empty();
    if (empty) return;
    Address same = current_;
    open(same);
}

void PageSession::close()
{
    stopWorker();
    js_.reset();
    applyDocument(kBlankLui);
    loading_ = false;
}

void PageSession::showDocument(const std::string& lui, const std::string& status)
{
    stopWorker();
    js_.reset();
    loading_ = false;
    failed_ = false;
    pending_ = PermissionRequest();
    const std::string error = applyDocument(lui);
    setStatus(error.empty() ? status : error);
}

void PageSession::stopWorker()
{
    abandon_.store(true);
    if (worker_.joinable()) worker_.join();
    abandon_.store(false);

    stopIconWorker();     // 换页/关页：在飞的那张图标作废

    std::lock_guard<std::mutex> lock(mutex_);
    result_ = HttpResult();
    resultReady_ = false;
}

void PageSession::startLoad(const Address& addr)
{
    if (worker_.joinable()) worker_.join();
    abandon_.store(false);

    worker_ = std::thread([this, addr] {
        HttpResult r = LoadResource(addr, 8000);
        if (abandon_.load()) return;          // 用户已经换页了，结果别往界面上盖
        std::lock_guard<std::mutex> lock(mutex_);
        result_ = std::move(r);
        resultReady_ = true;
    });
}

void PageSession::stopIconWorker()
{
    iconAbandon_.store(true);
    if (iconWorker_.joinable()) iconWorker_.join();
    iconAbandon_.store(false);
    std::lock_guard<std::mutex> lock(iconMutex_);
    iconPath_.clear();
    iconReady_ = false;
}

// 图标是「锦上添花」：单独一趟线程、超时更短、失败就当没有。不能塞进 startLoad ——
// 那样标签会一直转圈等图标，页面白屏的时间被图标拖长。
void PageSession::startIconLoad(const Address& addr)
{
    stopIconWorker();
    Address iconAddr = addr;
    iconAddr.path = "/favicon.ico";

    iconWorker_ = std::thread([this, iconAddr] {
        HttpResult r = HttpGet(iconAddr, 4000);
        if (iconAbandon_.load() || !r.ok || r.status != 200 || r.body.empty()) return;
        std::string path = SaveFaviconImage(r.body);
        if (path.empty() || iconAbandon_.load()) return;
        std::lock_guard<std::mutex> lock(iconMutex_);
        iconPath_ = std::move(path);
        iconReady_ = true;
    });
}

std::string PageSession::takeIconPath()
{
    std::lock_guard<std::mutex> lock(iconMutex_);
    if (!iconReady_) return std::string();
    iconReady_ = false;
    return iconPath_;
}

bool PageSession::poll()
{
    if (!loading_) return false;

    HttpResult r;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!resultReady_) return false;
        r = std::move(result_);
        result_ = HttpResult();
        resultReady_ = false;
    }
    if (worker_.joinable()) worker_.join();

    loading_ = false;

    if (!r.ok) {
        showError("打不开 " + current_.url() + "：" + r.error);
        return true;
    }
    if (r.status != 200) {
        showError("HTTP " + std::to_string(r.status) + " " + r.reason + "  " + current_.url());
        return true;
    }
    if (r.body.empty()) {
        showError("响应是空的：" + current_.url());
        return true;
    }

    // 本地 .lui：文件本身就是界面，没有脚本可跑（拖一个 .lui 进来 = 直接看这份文档）
    if (current_.scheme == Scheme::File && hasSuffix(current_.path, ".lui")) {
        showDocument(r.body, "就绪 " + current_.url());
        LOPPE_INFO("[page] doc " << current_.url() << " (" << r.body.size() << " B)");
        return true;
    }

    if (!runScript(r.body)) return true;
    setStatus("就绪 " + current_.url());
    LOPPE_INFO("[page] loaded " << current_.url() << " (" << r.body.size() << " B)");
    // 页面已经上屏（loading_ 已落 false），图标这才开始抓 —— 标签不会为它多转一圈。
    // 本地文件没有站点图标，不抓。
    if (current_.scheme == Scheme::Http) startIconLoad(current_);
    return true;
}

bool PageSession::runScript(const std::string& source)
{
    js_ = std::make_unique<loppe_script::ScriptHost>();

    loppe_script::ScriptHost::Config cfg;
    cfg.memoryLimitBytes = 16u * 1024 * 1024;   // 桌面比板上宽松
    cfg.maxStackBytes = 512u * 1024;
    cfg.interruptMs = 500;                      // 单次执行连续占用超过 500ms 就中断
    cfg.maxJobsPerPump = 64;
    cfg.maxTimersPerPump = 32;

    if (!js_->init(scene_, runtime_, cfg)) {
        js_.reset();
        showError("页面引擎初始化失败");
        return false;
    }

    // 页面文档的落地口：脚本 ui.render(lui) 交出来的全文在这里解析成文档
    js_->setDocumentHandler([this](const std::string& lui) { return applyDocument(lui); });
    js_->setNetHandler([this](const std::string& url, std::string* error) {
        return handleNet(url, error);
    });
    js_->setResourceHandler([this](const std::string& url, std::string* error) {
        return fetchScript(url, error);
    });
    // ui.navigate 每次都重装：js_ 是随页面一起新建的（setNavigateHandler 早于本函数调用时
    // 事件已经丢了，所以这里必须再挂一次）
    if (navigateHandler_) js_->setNavigateHandler(navigateHandler_);

    // 页面需要知道自己在哪：`__page__.url` / `__page__.origin`。
    // 单独 eval 一次（不拼进 app.js），这样脚本里的行号仍然是 app.js 自己的行号。
    const std::string prelude =
        "var __page__ = { url: \"" + dslEscape(current_.url()) + "\", origin: \"" +
        dslEscape(current_.origin()) + "\" };";
    std::string preludeError;
    if (!js_->eval(prelude, "page.js", &preludeError)) {
        js_.reset();
        showError("页面环境初始化失败：" + preludeError);
        return false;
    }

    std::string document;
    std::string err;
    // filename 传脚本 URL：模块解析（import './x.js'）以它为基准
    if (!js_->evalDocument(source, current_.url().c_str(), &document, &err)) {
        js_.reset();
        showError("app.js 执行失败：" + err);
        return false;
    }

    // 返回式写法：脚本没调 ui.render，最后一个表达式是 .lui 文本
    if (!document.empty()) {
        const std::string docError = applyDocument(document);
        if (!docError.empty()) {
            js_.reset();
            showError("页面文档应用失败：" + docError);
            return false;
        }
    }
    return true;
}

std::string PageSession::applyDocument(const std::string& lui)
{
    if (!runtime_ || !scene_) return "页面没有绑定文档";

    std::shared_ptr<ASTNode> ast;
    try {
        loppe::LoppeParser parser;
        ast = parser.parse(lui).root();
    } catch (const std::exception& ex) {
        return std::string("文档解析失败：") + ex.what();
    }
    if (!ast) return "文档解析结果是空的";

    // 旧根马上会被换掉：场景视图先解引用（不销毁树），这一帧的 pass 会 adopt 新根
    if (auto* view = scene_->getResolvedTree()) view->clear();
    // notifyASTUpdated 内部已经 scheduler()->requestASTRepaint()：计划留在调度器里，
    // 由渲染循环本帧的 onApplyUpdates 消费（tick 挂在 onFrameBegin，早于它）。
    // 计划里的 PassImpact 是「帧末要 compose 谁」的唯一凭据，自己提前 apply 会把它吞掉。
    runtime_->notifyASTUpdated(ast);

    if (!host_) {
        runtime_->runPipelineRenderTreeOnly(loppe::ViewportSizeDp{ { 1100 }, { 700 } });
        if (auto* view = scene_->getResolvedTree()) {
            if (auto* engineTree = runtime_->getResolvedTree()) view->adoptRootFrom(*engineTree);
        }
    }
    if (js_) js_->invalidateElements();   // 文档整份替换：Element 句柄缓存作废
    return std::string();
}

bool PageSession::pumpJs()
{
    if (!js_) return false;
    return js_->pump();
}

std::string PageSession::fetchScript(const std::string& url, std::string* error)
{
    // 页面自己的代码/资源图（import 进来的 .js/.lui），信任级别同页面脚本本身：
    // 走页面加载通道，不查 net 权限。模块名已经是绝对 URL（normalize 阶段拼好了）。
    HostMap empty;
    // Auto：命中本地映射表走映射（loppe → 127.0.0.1:8080），没命中交 DNS
    ResolveOutcome parsed = ParseAddress(url, AddressMode::Auto, hostMap_ ? *hostMap_ : empty);
    if (!parsed.ok) {
        *error = parsed.error;
        return std::string();
    }
    HttpResult r = LoadResource(parsed.address, 5000);
    if (!r.ok) {
        *error = r.error;
        return std::string();
    }
    if (r.status != 200) {
        *error = "HTTP " + std::to_string(r.status);
        return std::string();
    }
    return r.body;
}

std::string PageSession::handleNet(const std::string& url, std::string* error)
{
    HostMap empty;
    ResolveOutcome parsed;
    if (!url.empty() && (url[0] == '/' || url.rfind("./", 0) == 0 || url.rfind("../", 0) == 0)) {
        // 相对当前页面：net.get("/data.json") / net.get("./data.json")
        parsed = ResolveLocation(current_, url);
    } else {
        parsed = ParseAddress(url, AddressMode::Auto, hostMap_ ? *hostMap_ : empty);
    }
    if (!parsed.ok) {
        *error = parsed.error + "（net.get 需要完整地址或以 / 开头的页面内路径）";
        return std::string();
    }

    // 本地文件不走权限门：它读的是用户自己拖进来的目录，不是某个站点。
    // 本地页面里 net.get("http://…") 仍然是网络请求，照常按页面 origin 问权限。
    if (parsed.address.scheme == Scheme::Http) {
        if (!perms_ || perms_->query(Capability::Network) != Decision::Allowed) {
            if (perms_ && !perms_->asked(Capability::Network)) {
                pending_.pending = true;
                pending_.origin = perms_->origin();
                pending_.capability = Capability::Network;
            }
            *error = "PermissionDenied: " + (perms_ ? perms_->origin() : std::string("(无站点)")) +
                     " 没有「" + CapabilityName(Capability::Network) + "」权限";
            return std::string();
        }
    }

    HttpResult r = LoadResource(parsed.address, 5000);
    if (!r.ok) {
        *error = r.error;
        return std::string();
    }
    if (r.status != 200) {
        *error = "HTTP " + std::to_string(r.status);
        return std::string();
    }
    return r.body;
}

void PageSession::setNavigateHandler(NavigateHandler handler)
{
    navigateHandler_ = std::move(handler);
    if (js_) js_->setNavigateHandler(navigateHandler_);
}

void PageSession::resolvePermission(bool allow)
{
    if (!pending_.pending || !perms_) {
        pending_ = PermissionRequest();
        return;
    }
    if (allow) perms_->grant(pending_.capability);
    else perms_->deny(pending_.capability);

    setStatus(allow ? ("已允许「" + std::string(CapabilityName(pending_.capability)) + "」，重新加载页面生效")
                    : ("已拒绝「" + std::string(CapabilityName(pending_.capability)) + "」"));
    pending_ = PermissionRequest();
}

void PageSession::showError(const std::string& message)
{
    failed_ = true;
    setStatus(message);
    LOPPE_ERROR("[page] " << message);

    const std::string lui =
        "@style {\n"
        "    .page { width: 100%; height: 100%; background-color: #ffffff; padding: 16px; }\n"
        "    .err-title { font-size: 20px; color: #101828; }\n"
        "    .err-body { font-size: 13px; color: #c0392b; }\n"
        "    .err-tip { font-size: 13px; color: #5b6b85; }\n"
        "}\n"
        "@layout {\n"
        "    .page { display: flex; flex-direction: column; gap: 8px; }\n"
        "}\n"
        "panel(class=\"page\") {\n"
        "    text(class=\"err-title\", value=\"打不开这个页面\"),\n"
        "    text(class=\"err-body\", value=\"" + dslEscape(message) + "\"),\n"
        "    text(class=\"err-tip\", value=\"检查地址，或在地址栏换一个页面。\")\n"
        "}\n";
    applyDocument(lui);
}

} // namespace loppe_browser
