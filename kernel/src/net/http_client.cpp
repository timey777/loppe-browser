// ===== 设计说明 =====
//
// 【为什么自己写 socket 而不是用系统 HTTP 栈】浏览器的核心是「请求怎么发出去的」：
// 端口/路径/重定向/超时都要能被观察和改写（后面还要加权限门、代理、离线缓存）。
// WinHTTP 之类的抽象层把这些藏起来，反而挡路；而 HTTPS 需要 TLS 库，
// 这一版只做明文 http，所以手写只有 400 行，依赖为零。
//
// 【为什么每个重定向都重连】keep-alive 要维护连接复用、半关闭、pipelining 的边界，
// 收益只有几毫秒；重定向本来就不多。Connection: close 是最不容易出错的写法。
//
// 【为什么整体超时按 deadline 算】单次 recv 超时防不住「服务端每 100ms 吐一个字节」，
// 那种情况下用户等的是永远转不完的圈。所有等待都拿同一个 deadline 卡。

#include "net/http_client.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
using socket_t = int;
static constexpr socket_t kInvalidSocket = -1;
#endif

namespace loppe_browser {
namespace {

using Clock = std::chrono::steady_clock;

std::string lowerAscii(std::string s)
{
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

std::string trimAscii(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

int remainingMs(Clock::time_point deadline)
{
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
    return left > 0 ? static_cast<int>(left) : 0;
}

bool ensureSocketLayer(std::string* error)
{
#if defined(_WIN32)
    static bool ready = [] {
        WSADATA d{};
        return WSAStartup(MAKEWORD(2, 2), &d) == 0;
    }();
    if (!ready) {
        *error = "网络初始化失败（WSAStartup）";
        return false;
    }
#else
    (void)error;
#endif
    return true;
}

void closeSocket(socket_t s)
{
    if (s == kInvalidSocket) return;
#if defined(_WIN32)
    closesocket(s);
#else
    ::close(s);
#endif
}

bool setNonBlocking(socket_t s, bool on)
{
#if defined(_WIN32)
    u_long mode = on ? 1u : 0u;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(s, F_SETFL, on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) == 0;
#endif
}

bool waitReadable(socket_t s, int ms)
{
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(s, &rd);
    timeval tv{};
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    const int r = select(static_cast<int>(s) + 1, &rd, nullptr, nullptr, &tv);
    return r > 0;
}

bool waitWritable(socket_t s, int ms)
{
    fd_set wr;
    FD_ZERO(&wr);
    FD_SET(s, &wr);
    timeval tv{};
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    const int r = select(static_cast<int>(s) + 1, nullptr, &wr, nullptr, &tv);
    return r > 0;
}

// 连上 host:port；host 为空时用 connectHost。成功返回 socket，失败返回 kInvalidSocket 并写 error。
socket_t connectTcp(const std::string& host, int port, int timeoutMs, std::string* error)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* res = nullptr;
    const std::string service = std::to_string(port);
    const int gai = getaddrinfo(host.c_str(), service.c_str(), &hints, &res);
    if (gai != 0 || !res) {
        *error = "解析主机失败：" + host;
        return kInvalidSocket;
    }

    socket_t good = kInvalidSocket;
    std::string lastError = "连接失败：" + host + ":" + service;

    for (addrinfo* it = res; it; it = it->ai_next) {
        socket_t s = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (s == kInvalidSocket) continue;

        setNonBlocking(s, true);
        int rc = ::connect(s, it->ai_addr, static_cast<int>(it->ai_addrlen));
        bool connected = (rc == 0);
        if (!connected) {
            const bool inProgress =
#if defined(_WIN32)
                WSAGetLastError() == WSAEWOULDBLOCK;
#else
                errno == EINPROGRESS;
#endif
            if (inProgress && waitWritable(s, timeoutMs)) {
                int soError = 0;
                socklen_t len = sizeof(soError);
                if (getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soError), &len) == 0 && soError == 0) {
                    connected = true;
                }
            }
        }

        if (connected) {
            setNonBlocking(s, false);
            int noDelay = 1;
            setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
            good = s;
            break;
        }
        closeSocket(s);
    }

    freeaddrinfo(res);
    if (good == kInvalidSocket) *error = lastError;
    return good;
}

bool sendAll(socket_t s, const std::string& data, Clock::time_point deadline, std::string* error)
{
    size_t sent = 0;
    while (sent < data.size()) {
        const int left = remainingMs(deadline);
        if (left <= 0) {
            *error = "发送请求超时";
            return false;
        }
        setNonBlocking(s, true);
        const int n = ::send(s, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        setNonBlocking(s, false);
        if (n > 0) {
            sent += static_cast<size_t>(n);
            continue;
        }
        if (!waitWritable(s, std::min(left, 500))) continue;
    }
    return true;
}

std::string decodeChunked(const std::string& raw, bool* ok)
{
    std::string out;
    size_t pos = 0;
    *ok = false;
    while (pos < raw.size()) {
        const size_t lineEnd = raw.find("\r\n", pos);
        if (lineEnd == std::string::npos) return out;
        const std::string sizeLine = raw.substr(pos, lineEnd - pos);
        const size_t semi = sizeLine.find(';');
        const std::string hex = trimAscii(semi == std::string::npos ? sizeLine : sizeLine.substr(0, semi));
        char* end = nullptr;
        const unsigned long long size = std::strtoull(hex.c_str(), &end, 16);
        if (end == hex.c_str()) return out;
        pos = lineEnd + 2;
        if (size == 0) {
            *ok = true;
            return out;
        }
        if (pos + size > raw.size()) return out;
        out.append(raw, pos, static_cast<size_t>(size));
        pos += static_cast<size_t>(size);
        if (raw.compare(pos, 2, "\r\n") == 0) pos += 2;
    }
    return out;
}

struct RawResponse {
    bool ok = false;
    std::string error;
    int status = 0;
    std::string reason;
    std::map<std::string, std::string> headers;
    std::string body;
};

RawResponse singleRequest(const Address& addr, int timeoutMs)
{
    RawResponse out;
    std::string error;
    if (!ensureSocketLayer(&error)) {
        out.error = error;
        return out;
    }

    const Clock::time_point deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    const std::string host = addr.connectHost.empty() ? addr.host : addr.connectHost;

    socket_t s = connectTcp(host, addr.port, timeoutMs, &error);
    if (s == kInvalidSocket) {
        out.error = error;
        return out;
    }

    std::string request = "GET " + (addr.path.empty() ? std::string(kDefaultPath) : addr.path) + " HTTP/1.1\r\n";
    request += "Host: " + addr.host;
    if (addr.port != kDefaultPort) request += ":" + std::to_string(addr.port);
    request += "\r\n";
    request += "User-Agent: LoppeBrowser/0.1\r\n";
    request += "Accept: */*\r\n";
    request += "Connection: close\r\n\r\n";

    if (!sendAll(s, request, deadline, &error)) {
        closeSocket(s);
        out.error = error;
        return out;
    }

    std::string raw;
    std::vector<char> buf(16384);
    for (;;) {
        const int left = remainingMs(deadline);
        if (left <= 0) {
            out.error = "等待响应超时";
            closeSocket(s);
            return out;
        }
        if (!waitReadable(s, std::min(left, 500))) continue;

        const int n = ::recv(s, buf.data(), static_cast<int>(buf.size()), 0);
        if (n > 0) {
            raw.append(buf.data(), static_cast<size_t>(n));
            continue;
        }
        if (n == 0) break;   // 对端关闭 = 响应结束
#if defined(_WIN32)
        const int last = WSAGetLastError();
        if (last == WSAEWOULDBLOCK) continue;
#else
        if (errno == EINTR || errno == EAGAIN) continue;
#endif
        break;
    }
    closeSocket(s);

    const size_t headEnd = raw.find("\r\n\r\n");
    if (headEnd == std::string::npos) {
        out.error = raw.empty() ? std::string("服务端没有返回任何数据") : std::string("响应头不完整");
        return out;
    }

    const std::string head = raw.substr(0, headEnd);
    const std::string rest = raw.substr(headEnd + 4);

    std::istringstream hs(head);
    std::string line;
    if (!std::getline(hs, line)) {
        out.error = "响应状态行缺失";
        return out;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    {
        std::istringstream sl(line);
        std::string version;
        sl >> version >> out.status;
        std::getline(sl, out.reason);
        out.reason = trimAscii(out.reason);
        if (out.status <= 0) {
            out.error = "状态行无法解析：" + line;
            return out;
        }
    }

    while (std::getline(hs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        out.headers[lowerAscii(trimAscii(line.substr(0, colon)))] = trimAscii(line.substr(colon + 1));
    }

    auto headerValue = [&out](const char* key) -> std::string {
        auto it = out.headers.find(key);
        return it == out.headers.end() ? std::string() : it->second;
    };

    const std::string transfer = lowerAscii(headerValue("transfer-encoding"));
    if (transfer.find("chunked") != std::string::npos) {
        bool decoded = false;
        out.body = decodeChunked(rest, &decoded);
        if (!decoded) {
            out.error = "分块传输内容不完整";
            return out;
        }
    } else {
        const std::string len = headerValue("content-length");
        if (!len.empty()) {
            const unsigned long long n = std::strtoull(len.c_str(), nullptr, 10);
            out.body = rest.size() >= n ? rest.substr(0, static_cast<size_t>(n)) : rest;
        } else {
            out.body = rest;
        }
    }

    out.ok = true;
    return out;
}

} // namespace

HttpResult HttpGet(const Address& addr, int timeoutMs, int maxRedirects)
{
    HttpResult result;
    result.finalUrl = addr.url();

    Address current = addr;
    for (int hop = 0;; ++hop) {
        RawResponse raw = singleRequest(current, timeoutMs);
        if (!raw.ok) {
            result.error = raw.error;
            return result;
        }

        const bool isRedirect = (raw.status == 301 || raw.status == 302 || raw.status == 303 ||
                                 raw.status == 307 || raw.status == 308);
        auto loc = raw.headers.find("location");
        if (isRedirect && loc != raw.headers.end() && hop < maxRedirects) {
            ResolveOutcome next = ResolveLocation(current, loc->second);
            if (!next.ok) {
                result.error = "重定向地址不合法：" + next.error;
                return result;
            }
            current = next.address;
            result.redirects = hop + 1;
            result.finalUrl = current.url();
            continue;
        }

        result.ok = true;
        result.status = raw.status;
        result.reason = raw.reason;
        result.body = std::move(raw.body);
        auto ct = raw.headers.find("content-type");
        if (ct != raw.headers.end()) result.contentType = ct->second;
        result.finalUrl = current.url();
        return result;
    }
}

} // namespace loppe_browser
