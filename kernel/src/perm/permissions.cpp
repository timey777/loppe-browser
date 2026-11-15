// ===== 设计说明 =====
//
// 【为什么先做「存储 + 默认拒绝」而不做弹窗流】权限系统的难点不在弹窗，而在
// 「谁在什么时候能以什么为理由要什么」这条链路。先把 origin→能力→决定 这张表
// 钉死（并且默认拒绝），后面加能力只是加枚举和调用点；如果先做 UI，很容易变成
// 「到处 if (允许)」的散装判断。
//
// 【为什么拒绝也要落盘】不然用户每次刷新都再被问一次，最后他会习惯性点「允许」——
// 这比默认拒绝更危险。
//
// 【为什么键是 host:port 而不是 host】同一台机器上 80 和 8080 可能是两个完全不同的
// 东西（本地映射表里尤其常见），授权不能互相借。

#include "perm/permissions.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace loppe_browser {
namespace {

std::string trimAscii(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

} // namespace

const char* CapabilityName(Capability c)
{
    switch (c) {
        case Capability::Storage: return "存储";
        case Capability::Network: break;
    }
    return "网络";
}

const char* CapabilityId(Capability c)
{
    switch (c) {
        case Capability::Storage: return "storage";
        case Capability::Network: break;
    }
    return "network";
}

std::string PermissionStore::key(const std::string& origin, Capability c)
{
    return origin + "|" + CapabilityId(c);
}

void PermissionStore::load(const std::string& file)
{
    file_ = file;
    decisions_.clear();
    asked_.clear();

    std::ifstream f(file, std::ios::binary);
    if (!f) return;

    std::string line;
    while (std::getline(f, line)) {
        const std::string raw = trimAscii(line);
        if (raw.empty() || raw[0] == '#') continue;

        std::istringstream ss(raw);
        std::string origin;
        std::string cap;
        std::string decision;
        ss >> origin >> cap >> decision;
        if (origin.empty() || cap.empty() || decision.empty()) continue;

        Capability c = Capability::Network;
        if (cap == "storage") c = Capability::Storage;
        else if (cap != "network") continue;

        decisions_[key(origin, c)] = (decision == "allow") ? Decision::Allowed : Decision::Denied;
    }
}

void PermissionStore::setOrigin(const std::string& origin)
{
    origin_ = origin;
}

Decision PermissionStore::query(Capability c) const
{
    if (origin_.empty()) return Decision::Denied;
    auto it = decisions_.find(key(origin_, c));
    return it == decisions_.end() ? Decision::Denied : it->second;
}

bool PermissionStore::asked(Capability c) const
{
    auto it = asked_.find(key(origin_, c));
    return it != asked_.end() && it->second;
}

void PermissionStore::grant(Capability c)
{
    if (origin_.empty()) return;
    decisions_[key(origin_, c)] = Decision::Allowed;
    asked_[key(origin_, c)] = true;
    save();
}

void PermissionStore::deny(Capability c)
{
    if (origin_.empty()) return;
    decisions_[key(origin_, c)] = Decision::Denied;
    asked_[key(origin_, c)] = true;
    save();
}

void PermissionStore::resetRuntimePrompts()
{
    asked_.clear();
}

void PermissionStore::save() const
{
    std::ofstream f(file_, std::ios::binary | std::ios::trunc);
    if (!f) return;
    f << "# loppe-browser 站点权限（origin capability allow|deny）\n";
    f << "# origin = host:port；没有记录的站点一律拒绝\n";
    for (const auto& kv : decisions_) {
        const size_t bar = kv.first.rfind('|');
        if (bar == std::string::npos) continue;
        f << kv.first.substr(0, bar) << ' ' << kv.first.substr(bar + 1) << ' '
          << (kv.second == Decision::Allowed ? "allow" : "deny") << '\n';
    }
}

} // namespace loppe_browser
