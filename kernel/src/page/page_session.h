#pragma once
// page_session.h —— 一个「页面」的完整生命周期：抓 app.js → 在沙箱里执行 → 换页时收干净。
//
// 页面的界面是一份 .lui 文档：脚本调 ui.render(lui) 交出来（或让最后一个表达式就是文档），
// 宿主解析后整份替换这个标签的文档。没有「容器 + 逐个造控件」这层。
//
// 用法（宿主每帧调用两次，顺序无关）：
//     page_.open(addr);              // 用户按回车
//     page_.showDocument(lui, "新标签页");   // 本地文档（开始页 / 错误页），不走网络
//     while (...) {
//         if (page_.pumpJs()) { /* JS 跑过任务，脏区已标好 */ }
//         if (page_.poll())   { /* 这一帧状态变了：读 status()/failed() 刷状态栏 */ }
//     }
//
// 约定：
//   - open/reload/poll/pumpJs 全在主线程调用；抓取在内部工作线程，poll 只做交接。
//   - 页面脚本只能改自己这份文档里的元素（每个标签一份 Scene + 一棵解析树）。
//   - 页面脚本的 net.get 一律先过权限表；没授权就抛 PermissionDenied，
//     同时把请求挂到 pendingPermission() 让宿主去问用户。

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "net/http_client.h"
#include "net/http_url.h"
#include "perm/permissions.h"

namespace loppe {
class ViewCoordinator;
}

class ScriptScene;
class DocumentRuntime;

namespace loppe_script {
class ScriptHost;
}

namespace loppe_browser {

class PageSession {
public:
    struct PermissionRequest {
        bool pending = false;
        std::string origin;
        Capability capability = Capability::Network;
    };

    // host = 这个标签的视图协调器：应用文档后由它跑一趟全量（管线 + 控件同步 + 渲染），
    // 这样脚本里 ui.render 之后的 ui.set 能立刻命中控件。
    PageSession(ScriptScene* scene, DocumentRuntime* runtime, loppe::ViewCoordinator* host,
                PermissionStore* perms, const HostMap* hostMap = nullptr);
    ~PageSession();

    PageSession(const PageSession&) = delete;
    PageSession& operator=(const PageSession&) = delete;

    // 打开一个地址：先收掉上一页（JS 上下文 + 文档），再起后台抓取。
    void open(const Address& addr);
    // 重开当前地址（权限变更后、用户点刷新时用）。
    void reload();
    // 收掉当前页（不改变地址记录）。
    void close();

    // 直接加载一份本地文档（开始页、错误页）：不走网络、不起 JS 上下文。
    void showDocument(const std::string& lui, const std::string& status);

    // 主线程每帧调用。返回 true = 这一帧状态有变化（加载完成 / 失败 / 执行完）。
    bool poll();
    // 主线程每帧调用。返回 true = 这一帧 JS 真的跑过微任务或定时器。
    bool pumpJs();

    bool loading() const { return loading_; }
    bool failed() const { return failed_; }
    const std::string& status() const { return status_; }
    const Address& address() const { return current_; }

    // 标签图标（favicon）：文档加载完成之后单独去抓一次 <origin>/favicon.ico 并落盘。
    // 宿主每帧取一次；返回非空 = 刚抓好了一个新图标（读一次即消费）。
    // 没有图标（抓不到 / 格式不支持 / 本地文档）就一直返回空 —— 宿主用默认图标。
    std::string takeIconPath();

    PermissionRequest pendingPermission() const { return pending_; }
    // 用户点了「允许 / 拒绝」：写进权限表（落盘），清掉待问请求。
    void resolvePermission(bool allow);

    // ui.navigate(url) 的出口：把页面里的跳转请求交给宿主。
    // 返回空串 = 已受理（真正换页在下一帧，不在这次回调里），非空 = 抛给脚本的原因。
    using NavigateHandler = std::function<std::string(const std::string& url)>;
    void setNavigateHandler(NavigateHandler handler);

private:
    void startLoad(const Address& addr);
    void stopWorker();
    // 图标单独一趟：文档加载完才起，抓不到就算了（不挡页面加载、也不影响状态栏）
    void startIconLoad(const Address& addr);
    void stopIconWorker();
    bool runScript(const std::string& source);
    // ui.render / evalDocument 的落地：解析 .lui → 换文档 → 跑一趟全量。
    std::string applyDocument(const std::string& lui);
    void showError(const std::string& message);
    std::string handleNet(const std::string& url, std::string* error);
    // 模块/脚本资源（import './x.js'）：页面自己的代码图，不查网络权限
    std::string fetchScript(const std::string& url, std::string* error);
    void setStatus(const std::string& text);

    ScriptScene* scene_ = nullptr;
    const HostMap* hostMap_ = nullptr;   // hosts.cfg 别名表（net/模块取资源解析主机用；空 = 只认公网名）
    DocumentRuntime* runtime_ = nullptr;
    loppe::ViewCoordinator* host_ = nullptr;
    PermissionStore* perms_ = nullptr;

    std::unique_ptr<loppe_script::ScriptHost> js_;
    Address current_;
    std::string status_ = "就绪";
    bool loading_ = false;
    bool failed_ = false;

    // 后台抓取
    std::thread worker_;
    mutable std::mutex mutex_;
    HttpResult result_;
    bool resultReady_ = false;
    std::atomic<bool> abandon_{ false };

    // 图标（第二个工作线程：页面已经上屏了，图标晚一点到不影响任何东西）
    std::thread iconWorker_;
    mutable std::mutex iconMutex_;
    std::string iconPath_;
    bool iconReady_ = false;
    std::atomic<bool> iconAbandon_{ false };

    PermissionRequest pending_;
    NavigateHandler navigateHandler_;
};

} // namespace loppe_browser
