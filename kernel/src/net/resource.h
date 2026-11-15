#pragma once
// resource.h —— 「按地址取一份字节」的唯一入口：http 走网络，file 读本地文件。
//
// 用法：
//     loppe_browser::HttpResult r = loppe_browser::LoadResource(address, 8000);
//     if (!r.ok) showError(r.error);
//     else if (r.status != 200) showError("HTTP " + std::to_string(r.status));
//     else run(r.body);
//
// 约定：
//   - 同步阻塞（与 HttpGet 一样），调用方自己放在工作线程里。
//   - 页面加载、模块 import、net.get 都走这里：调用点不需要知道地址是网络的还是本地的。
//   - 返回的 HttpResult 两种来源同义：status 200 = 取到了，body 是原始字节。

#include "net/http_client.h"   // 复用 HttpResult（ok / status / body / error 对两种来源同义）

namespace loppe_browser {

HttpResult LoadResource(const Address& addr, int timeoutMs = 8000);

} // namespace loppe_browser
