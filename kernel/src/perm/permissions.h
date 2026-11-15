#pragma once
// permissions.h —— 站点权限（预留层：先能问、能记、能拒，再谈细粒度）。
//
// 用法：
//     loppe_browser::PermissionStore perms;
//     perms.load("config/permissions.cfg");
//     perms.setOrigin(addr.origin());                 // 换页时切站点
//     if (perms.query(loppe_browser::Capability::Network) != loppe_browser::Decision::Allowed)
//         askUser();                                  // 页面脚本要联网 → 先问
//     perms.grant(loppe_browser::Capability::Network); // 用户点了「允许」→ 记住并落盘
//
// 规则：
//   - 没有记录的站点 = 全部拒绝（默认拒绝，不是默认允许）。
//   - 授权按 origin（host:port）归档，跨进程保留；换端口算另一个站点。
//   - 拒绝也会记下来，避免同一个站点每次请求都弹一次窗。
//
// 现在真正被门挡住的只有 Capability::Network（页面脚本的 net.get）。
// 其余能力先留着位子：加能力时只加枚举 + 调用点，不动存储格式。

#include <map>
#include <string>

namespace loppe_browser {

enum class Capability {
    Network = 0,   // 页面脚本访问网络（net.get）
    Storage = 1,   // 留位：页面本地存储
};

const char* CapabilityName(Capability c);   // 给人看："网络" / "存储"
const char* CapabilityId(Capability c);     // 存盘用："network" / "storage"

enum class Decision {
    Denied = 0,
    Allowed = 1,
};

class PermissionStore {
public:
    // 读文件；不存在 = 空表（一切都还是拒绝）。
    void load(const std::string& file);
    const std::string& file() const { return file_; }

    void setOrigin(const std::string& origin);
    const std::string& origin() const { return origin_; }

    Decision query(Capability c) const;
    bool asked(Capability c) const;   // 本次运行里这个站点是否已经问过（拒绝也算）
    void grant(Capability c);         // allow + 落盘
    void deny(Capability c);          // deny + 落盘

    // 换页时清掉「问过」标记：新页面值得再问一次。
    void resetRuntimePrompts();

private:
    static std::string key(const std::string& origin, Capability c);
    void save() const;

    std::string file_;
    std::string origin_;
    std::map<std::string, Decision> decisions_;   // key = origin|capability
    std::map<std::string, bool> asked_;           // key = origin|capability（只在本进程内）
};

} // namespace loppe_browser
