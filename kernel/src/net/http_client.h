#pragma once
// http_client.h —— 明文 HTTP/1.1 的同步 GET。
//
// 用法：
//     loppe_browser::Address addr = ...;                    // 见 net/http_url.h
//     loppe_browser::HttpResult r = loppe_browser::HttpGet(addr, 8000);
//     if (!r.ok) showError(r.error);                        // 传输失败（连不上/超时）
//     else if (r.status != 200) showError("HTTP " + std::to_string(r.status));
//     else run(r.body);
//
// 约定：
//   - 同步阻塞，调用方自己放在工作线程里（页面加载就是这么做的）。
//   - 只支持 http（明文）；https 在地址解析阶段就被拒了。
//   - 每跳一个重定向重新建连接，不共享连接池（简单，且不会踩 keep-alive 的坑）。
//   - 返回的 body 原样是服务端字节（不猜字符集，按 UTF-8 处理）。

#include <string>

#include "net/http_url.h"

namespace loppe_browser {

struct HttpResult {
    bool ok = false;          // 传输层成功拿到响应（true 不代表 2xx）
    int status = 0;
    std::string reason;       // 状态行里的原因短语
    std::string contentType;  // Content-Type（可能为空）
    std::string body;
    std::string error;        // 传输失败原因（中文）
    std::string finalUrl;     // 跟随重定向之后的地址
    int redirects = 0;
};

// 一次性取回整个响应体。timeoutMs 是「单次连接/等待」的上限，也是整体上限的近似值。
HttpResult HttpGet(const Address& addr, int timeoutMs = 8000, int maxRedirects = 4);

} // namespace loppe_browser
