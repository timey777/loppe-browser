#include "browser_app.h"

#include <fstream>

#include "misc/filesystem.h"

namespace {
// exe 落点有两种布局：desktop/ 单独构建（exe 在 desktop/build/…）和解决方案根全量构建
// （exe 在根 build/…）。把 config/ 的绝对目录探出来传给内核，别赌相对路径。
std::string FindConfigDir()
{
    const char* kCandidates[] = {
        "config",                  // desktop/ 构建：exe/build 上两级 = desktop/
        "../config",               // 保险多探一层
        "../../config",            // 根构建：exe/build 上两级 = 仓库根（老布局）
        "../../desktop/config",    // 根构建 + 新布局：仓库根下的 desktop/
        "../../../desktop/config", // 再深一层的落点
    };
    for (const char* c : kCandidates) {
        const std::string dir = FileSystem::getPath(c);
        if (std::ifstream(dir + "/hosts.cfg", std::ios::binary)) return dir;
    }
    return {};   // 都没探到 = 交回内核默认探测（报错路径跟原来一致）
}

// 命令行第一个参数可能是地址栏写法（loppe / 域名 / http://），也可能是把文件拖到 exe 图标上
// 得到的一个路径 —— 后者包成 file:// 地址，剩下的交给地址栏那套解析。
std::string StartupAddressFrom(const char* arg)
{
    const std::string s = arg ? arg : "";
    if (s.empty() || s.find("://") != std::string::npos) return s;
    if (std::ifstream(s, std::ios::binary)) return "file://" + s;
    return s;
}
} // namespace

int main(int argc, char* argv[])
{
    // 渲染后端：默认 OpenGL；build.lpj 的 define 里加 LOPPE_RENDER_CPU 就编成纯 CPU 渲染。
#if defined(LOPPE_RENDER_CPU)
    SetRenderBackend(RenderBackend::Cpu);
#endif

    loppe_browser::BrowserApp app("Loppe Browser", 1100, 760);
    app.setStartupConfigDir(FindConfigDir());

    // 命令行给了地址就直接开它：LoppeBrowser.exe loppe:8080 / D:\dir\app.js
    if (argc > 1) app.setStartupAddress(StartupAddressFrom(argv[1]));

    return app.run(argc, argv);
}
