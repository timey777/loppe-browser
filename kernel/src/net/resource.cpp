// ===== 设计说明 =====
//
// 【为什么单独一层】页面加载、模块 import、net.get 三处原来都直接调 HttpGet，加本地文件时
// 若每处都写一遍「是不是 file://」的分支，往后再加 scheme 就得改三遍。这层只做一件事：
// 地址 → 字节，调用方按 scheme 分岔全部收在 LoadResource 里面。
//
// 【为什么复用 HttpResult】两种来源的语义完全对得上：ok = 取到了、status = 200、
// body = 原始字节、error = 给人看的中文。再定义一个几乎一样的结构只会让调用点多一个分支。

#include "net/resource.h"

#include <fstream>
#include <sstream>

namespace loppe_browser {
namespace {

HttpResult readLocalFile(const Address& addr)
{
    HttpResult r;
    r.finalUrl = addr.url();
    std::ifstream f(addr.path, std::ios::binary);
    if (!f) {
        r.error = "读不到本地文件：" + addr.path;
        return r;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    r.body = ss.str();
    r.status = 200;
    r.ok = true;
    return r;
}

} // namespace

HttpResult LoadResource(const Address& addr, int timeoutMs)
{
    if (addr.scheme == Scheme::File) return readLocalFile(addr);
    return HttpGet(addr, timeoutMs);
}

} // namespace loppe_browser
