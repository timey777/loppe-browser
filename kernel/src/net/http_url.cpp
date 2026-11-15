// ===== 设计说明 =====
//
// 【为什么解析和 DNS 分成两层】ParseAddress 只做字符串 → (host, port, path)，
// 不查表以外的东西。这样地址栏规则能被无头用例直接钉住，也不用为了「输入合不合法」
// 去连一次网。
//
// 【为什么 Auto 模式先查映射表】使用习惯上「输入一个不带点的短名字」几乎一定是本地
// 自建站点；带点/带 IP 的才像公网域名。Auto 就等于把这个习惯写成规则，而 Local /
// Domain 两个模式让用户可以强制只走其中一边（例如本地表里恰好有一条同名条目，
// 但这次想看真正的 DNS 结果）。

#include "net/http_url.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace loppe_browser {
namespace {

std::string trim(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool startsWithNoCase(const std::string& s, const char* prefix)
{
    const size_t n = std::char_traits<char>::length(prefix);
    if (s.size() < n) return false;
    return lower(s.substr(0, n)) == lower(std::string(prefix));
}

bool parsePort(const std::string& s, int* out)
{
    if (s.empty() || s.size() > 5) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    const int v = std::atoi(s.c_str());
    if (v <= 0 || v > 65535) return false;
    *out = v;
    return true;
}

ResolveOutcome fail(const std::string& why)
{
    ResolveOutcome r;
    r.ok = false;
    r.error = why;
    return r;
}

// 本地路径归一：反斜杠换正斜杠、压掉 file: 带出来的多余斜杠、
// Windows 盘符前那个斜杠去掉（url() 会按需要补回三条斜杠）。
std::string normalizeLocalPath(const std::string& raw)
{
    std::string s = trim(raw);
    for (char& c : s) {
        if (c == '\\') c = '/';
    }
    size_t cut = 0;
    while (cut + 1 < s.size() && s[cut] == '/' && s[cut + 1] == '/') ++cut;
    s = s.substr(cut);
    if (s.size() >= 3 && s[0] == '/' && std::isalpha(static_cast<unsigned char>(s[1])) && s[2] == ':') {
        s = s.substr(1);
    }
    return s;
}

bool isAbsolutePath(const std::string& s)
{
    if (s.empty()) return false;
    if (s[0] == '/') return true;                                     // POSIX
    return s.size() >= 3 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':' &&
           s[2] == '/';                                              // Windows 盘符
}

ResolveOutcome fileAddress(const std::string& rawPath)
{
    const std::string path = normalizeLocalPath(rawPath);
    if (path.empty()) return fail("本地文件路径是空的");
    if (!isAbsolutePath(path)) return fail("本地文件要用绝对路径：" + path);

    ResolveOutcome r;
    r.ok = true;
    r.address.scheme = Scheme::File;
    r.address.port = 0;
    r.address.path = path;
    return r;
}

} // namespace

const char* AddressModeName(AddressMode m)
{
    switch (m) {
        case AddressMode::Domain: return "domain";
        case AddressMode::Local: return "local";
        case AddressMode::Auto: break;
    }
    return "auto";
}

AddressMode AddressModeFromName(const std::string& s)
{
    const std::string v = lower(trim(s));
    if (v == "domain" || v == "dns") return AddressMode::Domain;
    if (v == "local" || v == "map") return AddressMode::Local;
    return AddressMode::Auto;
}

std::string Address::origin() const
{
    if (scheme == Scheme::File) return "file://";
    return lower(host) + ":" + std::to_string(port);
}

std::string Address::url() const
{
    if (scheme == Scheme::File) return path.empty() || path[0] == '/' ? "file://" + path : "file:///" + path;

    std::string s = "http://" + host;
    if (port != kDefaultPort) s += ":" + std::to_string(port);
    s += path.empty() ? std::string(kDefaultPath) : path;
    return s;
}

void HostMap::load(const std::string& file)
{
    file_ = file;
    entries_.clear();

    std::ifstream f(file, std::ios::binary);
    if (!f) return;

    std::string line;
    while (std::getline(f, line)) {
        const std::string raw = trim(line);
        if (raw.empty() || raw[0] == '#') continue;

        const size_t eq = raw.find('=');
        if (eq == std::string::npos) continue;

        const std::string name = lower(trim(raw.substr(0, eq)));
        std::string target = trim(raw.substr(eq + 1));
        if (name.empty() || target.empty()) continue;

        Entry e;
        const size_t colon = target.rfind(':');
        if (colon != std::string::npos && target.find(':') == colon) {
            int port = 0;
            if (parsePort(target.substr(colon + 1), &port)) {
                e.port = port;
                target = trim(target.substr(0, colon));
            }
        }
        if (target.empty()) continue;
        e.host = target;
        entries_[name] = e;
    }
}

bool HostMap::lookup(const std::string& name, std::string* host, int* port) const
{
    auto it = entries_.find(lower(trim(name)));
    if (it == entries_.end()) return false;
    if (host) *host = it->second.host;
    if (port) *port = it->second.port;
    return true;
}

ResolveOutcome ParseAddress(const std::string& input, AddressMode mode, const HostMap& map)
{
    std::string s = trim(input);
    if (s.empty()) return fail("地址栏是空的");

    // 本地文件：整串都是路径，不参与主机解析
    if (startsWithNoCase(s, "file:")) {
        std::string rest = s.substr(5);
        if (rest.rfind("//", 0) == 0) rest = rest.substr(2);
        return fileAddress(rest);
    }

    if (startsWithNoCase(s, "https://")) return fail("暂不支持 https（当前只实现明文 http）");
    if (startsWithNoCase(s, "http://")) s = s.substr(7);
    else {
        const size_t sep = s.find("://");
        if (sep != std::string::npos) return fail("不认识的协议：" + s.substr(0, sep));
    }

    std::string authority = s;
    std::string path;
    const size_t slash = s.find('/');
    if (slash != std::string::npos) {
        authority = s.substr(0, slash);
        path = s.substr(slash);
    }
    authority = trim(authority);
    if (authority.empty()) return fail("地址里没有主机");

    std::string host = authority;
    int port = kDefaultPort;
    bool explicitPort = false;
    const size_t colon = authority.rfind(':');
    if (colon != std::string::npos) {
        host = trim(authority.substr(0, colon));
        if (!parsePort(trim(authority.substr(colon + 1)), &port)) return fail("端口不合法：" + authority);
        explicitPort = true;
    }
    if (host.empty()) return fail("地址里没有主机");

    if (path.empty() || path == "/") path = kDefaultPath;

    Address addr;
    addr.host = host;
    addr.port = port;
    addr.path = path;

    std::string mappedHost;
    int mappedPort = kDefaultPort;
    const bool mapped = map.lookup(host, &mappedHost, &mappedPort);

    switch (mode) {
        case AddressMode::Local:
            if (!mapped) return fail("本地映射表里没有「" + host + "」（" + map.file() + "）");
            addr.connectHost = mappedHost;
            addr.fromAlias = true;
            if (!explicitPort) addr.port = mappedPort;
            break;
        case AddressMode::Domain:
            // 地址原样交给 DNS；本地映射表不参与。
            break;
        case AddressMode::Auto:
            if (mapped) {
                addr.connectHost = mappedHost;
                addr.fromAlias = true;
                if (!explicitPort) addr.port = mappedPort;
            }
            break;
    }

    ResolveOutcome r;
    r.ok = true;
    r.address = addr;
    return r;
}

ResolveOutcome ParseLocalFile(const std::string& path)
{
    return fileAddress(path);
}

ResolveOutcome ResolveLocation(const Address& base, const std::string& location)
{
    const std::string loc = trim(location);
    if (loc.empty()) return fail("重定向没有给出地址");

    if (startsWithNoCase(loc, "http://") || startsWithNoCase(loc, "https://") ||
        startsWithNoCase(loc, "file:")) {
        // 用 Domain + 空表：已经在跳转链路里，别再走一次本地别名（否则会被改写成别的站点）。
        HostMap empty;
        return ParseAddress(loc, AddressMode::Domain, empty);
    }

    Address next = base;
    std::string ref = loc;
    std::string dir;
    {
        const size_t cut = base.path.rfind('/');
        dir = (cut == std::string::npos) ? std::string("/") : base.path.substr(0, cut + 1);
    }

    // "./" 就地取；"../" 逐级上跳（只处理前缀，够页面内相对引用用了）
    if (ref.rfind("./", 0) == 0) ref = ref.substr(2);
    while (ref.rfind("../", 0) == 0) {
        ref = ref.substr(3);
        if (dir.size() > 1) {
            const std::string d = dir.substr(0, dir.size() - 1);
            const size_t cut = d.rfind('/');
            dir = (cut == std::string::npos) ? std::string("/") : d.substr(0, cut + 1);
        }
    }

    next.path = (!ref.empty() && ref[0] == '/') ? ref : dir + ref;

    ResolveOutcome r;
    r.ok = true;
    r.address = next;
    return r;
}

} // namespace loppe_browser
