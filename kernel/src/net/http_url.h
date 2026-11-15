#pragma once
// http_url.h —— 地址栏输入 → 一条可直接发出的 http 请求地址。
//
// 用法：
//     loppe_browser::HostMap map;
//     map.load("config/hosts.cfg");
//     auto r = loppe_browser::ParseAddress("loppe", loppe_browser::AddressMode::Auto, map);
//     if (!r.ok) showError(r.error);            // r.error 是给人看的中文
//     else request(r.address);
//
// 地址栏写法：`loppe`、`loppe:8080`、`127.0.0.1`、`192.168.1.7:9000/page.js`、
//   `http://example.com`。不写路径 = /app.js，不写端口 = 80。
//   `file:///D:/dir/app.js` 打开本地文件（拖进来的文件也走这条），见 ParseLocalFile。
//
// 三种模式只决定「host 怎么变成 IP」：
//   Auto   本地映射表命中就用它，否则交给 DNS（默认）
//   Domain 只走 DNS（本地映射表不参与）
//   Local  只查本地映射表，没命中就报错
//
// 本文件不做 DNS、不碰网络：解析只产出 host/port/path，真正连 IP 在 http_client。
// 本地文件地址在这层只是个「哪个文件」的结论，读盘在 net/resource.h。

#include <map>
#include <string>
#include <vector>

namespace loppe_browser {

// 地址背后的东西在哪：网络，还是本机磁盘
enum class Scheme {
    Http = 0,   // 明文 http（默认）
    File = 1,   // 本地文件：path 是磁盘绝对路径
};

// 地址栏「不写就是这些」
inline constexpr int kDefaultPort = 80;
inline constexpr const char* kDefaultPath = "/app.js";

enum class AddressMode {
    Auto = 0,     // 本地映射优先，其次 DNS
    Domain = 1,   // 只走 DNS
    Local = 2,    // 只走本地映射
};

const char* AddressModeName(AddressMode m);            // "auto" / "domain" / "local"
AddressMode AddressModeFromName(const std::string& s); // 认不出 → Auto

struct Address {
    Scheme scheme = Scheme::Http;   // File 时只认 path，host/port 不用
    std::string host;         // 地址栏里的主机：别名 / 域名 / IP；也是权限归档用的 origin 主体
    std::string connectHost;  // 真正连的地址；空 = 连 host（由 DNS 解析）
    int port = kDefaultPort;
    // Http：以 / 开头的请求路径；File：磁盘绝对路径（正斜杠，Windows 形如 D:/a/b.js）
    std::string path = kDefaultPath;
    bool fromAlias = false;   // 这个 host 是本地映射表给出来的

    // 权限归档键：host:port（小写）；本地文件统一 "file://"。同一个别名换端口算不同站点。
    std::string origin() const;
    // 给人看的完整地址：http://host[:port]/path（80 端口不写）；本地文件 file:///D:/a/b.js
    std::string url() const;
};

// 本地映射表：config/hosts.cfg，一行一条 `别名 = 主机[:端口]`。
class HostMap {
public:
    // 读文件；文件不存在 = 空表（不报错，Auto 模式照样能用域名）。
    void load(const std::string& file);
    bool lookup(const std::string& name, std::string* host, int* port) const;
    const std::string& file() const { return file_; }

private:
    struct Entry {
        std::string host;
        int port = kDefaultPort;
    };
    std::string file_;
    std::map<std::string, Entry> entries_;   // key 小写
};

struct ResolveOutcome {
    bool ok = false;
    std::string error;   // 失败原因（中文，可直接上状态栏）
    Address address;
};

// 解析地址栏输入。mode 只影响主机解析，其余规则三条模式完全一致。
// `file://…` 在这里就转成本地文件地址，不走主机解析。
ResolveOutcome ParseAddress(const std::string& input, AddressMode mode, const HostMap& map);

// 本地文件路径 → 地址（拖放 / 命令行拿到的都是这种裸路径）。Windows 的反斜杠也认；
// 只接受绝对路径。扩展名不在这里判断（能不能当页跑是宿主的规则）。
ResolveOutcome ParseLocalFile(const std::string& path);

// 重定向用：把 Location（可能是 "http://…" / "file://…" 绝对地址或 "/a/b" 相对路径）接到 base 上。
ResolveOutcome ResolveLocation(const Address& base, const std::string& location);

} // namespace loppe_browser
