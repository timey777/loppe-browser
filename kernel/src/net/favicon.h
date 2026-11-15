#pragma once
// favicon.h —— 把 /favicon.ico 的响应体变成「引擎能直接加载的图片文件」。
//
// 用法（在工作线程里调，会做磁盘写入）：
//     std::string path = loppe_browser::SaveFaviconImage(result.body);
//     if (path.empty()) { /* 没有可用图标：标签用默认图标 */ }
//     else { /* 交给 image 控件：preloadImage(path) + src = path */ }
//
// 支持的输入：
//   · PNG（有的站点在 /favicon.ico 上直接给 PNG）→ 原样落盘 .png
//   · ICO / CUR 容器 → 挑一张：PNG 载荷原样落盘 .png；BMP 载荷（32/24 位）解码后落盘 .tga
//   · 其它（含调色板 ICO、压缩 BMP）→ 返回空，调用方退回默认图标
//
// 为什么写 .tga 而不是 .png：引擎的位图解码是 stb_image，两种都认；TGA 的写入只要 18 字节头
// + BGRA，写 PNG 得自己实现 deflate —— 为了一个 16×16 的图标不值得。
//
// 为什么每次落一个新文件名：引擎按「路径」缓存纹理，同一个路径换了内容它不会重读。

#include <string>

namespace loppe_browser {

// 落盘一张图，返回绝对路径（失败返回空串）。文件名由内部计数器保证本次运行内唯一。
std::string SaveFaviconImage(const std::string& body);

// 本次运行的图标缓存目录（系统临时目录下的 loppe-browser-favicon）。
std::string FaviconCacheDir();

} // namespace loppe_browser
