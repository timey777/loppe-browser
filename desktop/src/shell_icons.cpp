// ===== 设计说明 =====
//
// 【为什么图标几何是手写的、不用 <g transform> 生成转圈帧】引擎的 SVG 光栅化走 nanosvg
// （render/svg/svg_registry.h），本仓没验证过它对 transform 属性的支持程度；转圈的 8 帧
// 干脆按角度把坐标算死（270° 弧、每帧顺时针 45°），注册成 8 份独立图形，渲染端只认名字。
//
// 【为什么注册点在外壳、不在 kernel】kernel 是三个壳（desktop / android / esp32）共用的，
// 不该依赖任一方的绘制资源。图标名是约定（kernel 侧拼 "shell-spin-N"，见 browser_scene.cpp），
// 各壳自己注册；没注册的壳上转圈不显示，其余功能不受影响。
//
// 【为什么全是白色】引擎把注册图标光栅化成「白色 RGB + 覆盖率 alpha」的掩码，颜色由使用处的
// CSS color 调制（renderer 的 renderSvgIcon）——所以这里写 #fff，换色去 main.lui。

#include "shell_icons.h"

#include <string>

#include "misc/logger.h"
#include "render/svg/svg_registry.h"

namespace loppe_browser {
namespace {

// 统一 24×24 viewBox、2px 描边、圆头：与编辑器那套 Lucide 图标同一套几何语言。
// 图标渲染时等比缩放居中撑满控件的边框盒（svg_registry 的 fitScale），所以显示多大
// 全看 main.lui 里那个 icon 的宽高；几何尽量写到 3.5~20.5，缩到 12~16dp 才不显瘦。
constexpr const char* kReloadSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 12a9 9 0 1 1-2.64-6.36"/><path d="M21 3v6h-6"/></svg>)svg";

// 标签关闭的 X：显示 14dp。窗口关闭另有一份贴边的几何（见 kWinCloseSvg）。
constexpr const char* kCloseSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2.2" stroke-linecap="round"><path d="M6 6l12 12M18 6l-12 12"/></svg>)svg";

constexpr const char* kWinCloseSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round"><path d="M3 3l18 18M21 3l-18 18"/></svg>)svg";

// 前进/后退：chevron。纵向撑到 4~20（缩到 16dp 约 11px 高），横向只有 8 格——
// 箭头本来就窄，硬撑到边会显得横向拉长。高度要跟刷新那个 14px 的圆环对得上。
constexpr const char* kBackSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M15.5 4l-8 8 8 8"/></svg>)svg";

constexpr const char* kForwardSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M8.5 4l8 8-8 8"/></svg>)svg";

// 窗口三键：Windows 11 那套——一条横线、一个方框、两个叠方框。几何贴到 3~21（正方形就是
// 17×17），显示尺寸 14dp 缩下来约 10px，正好是系统标题栏那个分量。
constexpr const char* kMinSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="butt"><path d="M3 12h18"/></svg>)svg";

constexpr const char* kMaxSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linejoin="miter"><path d="M3.5 3.5h17v17h-17z"/></svg>)svg";

constexpr const char* kRestoreSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linejoin="miter"><path d="M3.5 8.5h12v12h-12z"/><path d="M7.5 3.5h13v13"/></svg>)svg";

// 标签的默认图标：地球（有远端 favicon 或有加载转圈时会被替换掉）。
// 笔画都落在整数格上（圆心 12、半径 9，经线 3~21），缩到 14dp 才不糊。
constexpr const char* kTabGlobeSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="1.8" stroke-linecap="round"><circle cx="12" cy="12" r="9"/><path d="M3 12h18"/><ellipse cx="12" cy="12" rx="4.4" ry="9"/></svg>)svg";

// 转圈 8 帧：圆心 (12,12)、半径 9 的 270° 弧，每帧顺时针 45°（缺口就是「在转」的视觉锚点）。
// 坐标由 r=9、45° 步进算好写死：9*cos45 ≈ 6.36 → 12±6.36 = 18.36 / 5.64。
constexpr const char* kSpinnerSvg[8] = {
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2.6" stroke-linecap="round"><path d="M21 12A9 9 0 1 1 12 3"/></svg>)svg",
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2.6" stroke-linecap="round"><path d="M18.36 18.36A9 9 0 1 1 18.36 5.64"/></svg>)svg",
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2.6" stroke-linecap="round"><path d="M12 21A9 9 0 1 1 21 12"/></svg>)svg",
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2.6" stroke-linecap="round"><path d="M5.64 18.36A9 9 0 1 1 18.36 18.36"/></svg>)svg",
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2.6" stroke-linecap="round"><path d="M3 12A9 9 0 1 1 12 21"/></svg>)svg",
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2.6" stroke-linecap="round"><path d="M5.64 5.64A9 9 0 1 1 5.64 18.36"/></svg>)svg",
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2.6" stroke-linecap="round"><path d="M12 3A9 9 0 1 1 3 12"/></svg>)svg",
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2.6" stroke-linecap="round"><path d="M18.36 5.64A9 9 0 1 1 5.64 5.64"/></svg>)svg",
};

} // namespace

void RegisterShellIcons()
{
    bool ok = true;
    ok &= loppe::svg::RegisterIcon("shell-reload", kReloadSvg);
    ok &= loppe::svg::RegisterIcon("shell-close", kCloseSvg);
    ok &= loppe::svg::RegisterIcon("shell-win-close", kWinCloseSvg);
    ok &= loppe::svg::RegisterIcon("shell-back", kBackSvg);
    ok &= loppe::svg::RegisterIcon("shell-forward", kForwardSvg);
    ok &= loppe::svg::RegisterIcon("shell-min", kMinSvg);
    ok &= loppe::svg::RegisterIcon("shell-max", kMaxSvg);
    ok &= loppe::svg::RegisterIcon("shell-restore", kRestoreSvg);
    ok &= loppe::svg::RegisterIcon("shell-tab-globe", kTabGlobeSvg);
    for (int i = 0; i < 8; ++i) {
        ok &= loppe::svg::RegisterIcon("shell-spin-" + std::to_string(i), kSpinnerSvg[i]);
    }
    if (!ok) {
        LOPPE_WARN("[icon] 外壳图标注册失败（该图标会显示成空白）");
    }
}

} // namespace loppe_browser
