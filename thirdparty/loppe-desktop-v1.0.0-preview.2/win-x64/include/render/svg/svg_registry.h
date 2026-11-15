/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

// ═══════════════════════════════════════════════════════════════════
// render/svg/svg_registry.h — 引擎内置 SVG 图标注册表 + 光栅化
//
// 用法（应用侧）：
//   loppe::svg::RegisterIcon("close", "<svg ...>...</svg>");      // 启动时注册一次
//   displayList.addSvgIcon("close", bounds, tint);                // 控件/调用方发射指令
//
// 另有 **.svg 文件通路**（图片用）：`IsSvgPath` / `GetSvgFileSize` / `RasterizeSvgFileToRGBA`。
// ═══════════════════════════════════════════════════════════════════

#include <string>
#include <vector>
#include "loppe_export.h"

namespace loppe {
namespace svg {

// 注册命名 SVG 图标（同名覆盖）。明显非 SVG 的内容（不含 "<svg"）会被拒绝并返回 false
// （引擎日志 WARN）。
LOPPE_API bool RegisterIcon(const std::string& name, const std::string& svgText);

// 是否已注册。
LOPPE_API bool HasIcon(const std::string& name);

// 注册引擎自带的内置图标集：chevron-down / chevron-up / chevron-right / chevron-left。
// Application::run 启动时自动调用一次；已存在的同名图标不覆盖。
LOPPE_API void RegisterBuiltinIcons();

// 清空注册表并释放解析缓存。
LOPPE_API void ClearIcons();

// 光栅化命名图标 → 「白色 RGB + 覆盖率 alpha」RGBA（width*height*4 字节）。
// 图标等比缩放居中放进 width×height（不拉伸）；失败返回 false。
LOPPE_API bool RasterizeIconRGBA(const std::string& name, int width, int height,
                                 std::vector<unsigned char>& outRGBA);

// 通用 SVG 文本光栅化（保留原色、透明背景），供 .svg 文件走图片通路。
LOPPE_API bool RasterizeToRGBA(const std::string& svgText, int width, int height,
                               std::vector<unsigned char>& outRGBA);

// ── .svg 文件通路（图片用：内容测量 + 异步加载）──────────────

// 路径是否视为 .svg（仅看扩展名，大小写不敏感）
LOPPE_API bool IsSvgPath(const std::string& path);

// 读取 .svg 文件并返回其声明尺寸（viewBox 宽高，四舍五入）；失败 false。只读尺寸，不光栅化。
LOPPE_API bool GetSvgFileSize(const std::string& path, int& width, int& height);

// 读取 .svg 文件并光栅化成「原色 + 覆盖率 alpha」RGBA（供 image/位图通路）。
// 尺寸策略：声明尺寸 ×4 超采样，最长边收进 [64, 512]（等比；回传实际光栅尺寸）。
LOPPE_API bool RasterizeSvgFileToRGBA(const std::string& path,
                                      std::vector<unsigned char>& outRGBA,
                                      int& outWidth, int& outHeight);

// 光栅化 svg **文本**（调用方已把文件读成字符串时用）；尺寸策略同 RasterizeSvgFileToRGBA。
LOPPE_API bool RasterizeSvgTextToRGBA(const std::string& svgText,
                                      std::vector<unsigned char>& outRGBA,
                                      int& outWidth, int& outHeight);

} // namespace svg
} // namespace loppe
