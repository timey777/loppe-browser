/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// embedded_host.h — 嵌入式宿主门面：装配 + 每帧驱动 + 触摸翻译 + 脏区累加。
//
// 最小用法：
//     loppe_embedded::EmbeddedHost host;
//     loppe_embedded::EmbeddedHost::Config cfg;
//     cfg.width = 240; cfg.height = 320;           // 面板原生像素尺寸
//     cfg.rotation = 0;                            // 0 / 90 / 180 / 270（顺时针），90、270 为横屏
//     cfg.framebuffer = fb;                        // 宿主分配（PSRAM / 内部 RAM）
//     cfg.font = &loppe_embedded::kEmbeddedFont;   // 位图字库（嵌入式无 freetype）
//     if (!host.init(cfg)) { /* framebuffer / font 没给 */ }
//     if (!host.loadUi(luiBytes, luiSize)) { /* .lui 解析失败 */ }
//     for (;;) {
//         int x = 0, y = 0;
//         const bool touched = touch.read(x, y);   // x / y 是面板坐标
//         host.touchSample(touched, x, y);         // 触摸 → 鼠标事件（按下 / 移动 / 抬手 + 清指针）
//         host.tick();                             // 消费更新计划 + 有脏区才重画
//         if (host.needsFullFlush()) lcd.flush(host.framebuffer());
//         else for (const auto& r : host.dirtyRects())
//                  lcd.flushRegion(host.framebuffer(), int(r.x), int(r.y), int(r.width), int(r.height));
//         host.clearDamage();                      // 刷完清账
//     }
//
// 坐标口径：引擎一侧全部用**逻辑 DIP**（devicePixelRatio = 1 时逻辑 == 像素）；DPR ≠ 1 请自行换算。
// 唯一的例外是触摸输入：`touchSample/pointerXxx` 收**面板坐标**，内部按 `rotation` 换算成逻辑坐标。

#include "framebuffer.h"
#include "idrawer_soft.h"

#include "loppe.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace loppe_embedded {

class EmbeddedHost {
public:
    // 装配参数（含脏区策略）。framebuffer / font 是必填 —— 缺了 init() 直接失败，
    // 不猜「默认字体」也不替宿主分配显存（PSRAM/内部 RAM 只有板级知道）。
    struct Config {
        int width = 0;      // 面板原生像素宽（LCD 驱动坐标系）
        int height = 0;     // 面板原生像素高
        int rotation = 0;   // 顺时针旋转角度：0 / 90 / 180 / 270；90、270 时逻辑宽高互换
        uint16_t* framebuffer = nullptr;    // 由宿主分配，引擎只借用（像素数 = width * height）
        const BitmapFont* font = nullptr;   // 位图字库（嵌入式没有 freetype），如 &kEmbeddedFont
        uint16_t clearColor = 0x0000;       // 重画前的清屏色（RGB565）

        // 脏区策略（算法在 loppe::ui::DirtyRegion）—— 只影响「怎么刷屏划算」，不影响正确性
        int dirtyMargin = 0;        // 每块外扩像素（圆角/边缘取整保险）
        int maxDirtyRects = 8;      // 超过这么多块 → 整屏刷
        int fullFlushPercent = 0;   // 脏区面积 ≥ 整屏的百分之几 → 整屏刷（0 = 不按面积退化）
    };

    EmbeddedHost();
    ~EmbeddedHost();
    EmbeddedHost(const EmbeddedHost&) = delete;
    EmbeddedHost& operator=(const EmbeddedHost&) = delete;

    // 装配引擎 + 场景 + 协调器（幂等：重复调用返回 false）。失败 = 参数不全（见 Config 注释）
    bool init(const Config& cfg);
    bool ready() const { return ready_; }
    void setDirtyPolicy(int marginPx, int maxRects, int fullPercent);   // 运行期也能改

    // ===== 方向（横竖屏）=====
    // 逻辑尺寸 = 面板尺寸按 rotation 旋转后的视口：布局、脏区、叠加层坐标都用它。
    int logicalWidth() const { return logicalW_; }
    int logicalHeight() const { return logicalH_; }
    int rotation() const { return rotation_; }
    // 运行期换方向：整棵树重排，下一次 tick() 整屏重画。返回 false = 未就绪 / 角度非法 / 没变化。
    // 面板本身的方向（MADCTL 之类）由 LCD 驱动切，引擎只换逻辑视口与输入换算。
    bool setRotation(int degrees);

    // 界面：把 .lui 的字节交给引擎（解析失败返回 false，屏幕上保持原样）
    bool loadUi(const char* data, size_t size);
    bool loadUi(const std::string& source);

    // ===== 输入：驱动每次采样后调用一次（x / y 为面板坐标）=====
    // touched 是**电平**语义（「本帧摸到了没有」），不是边沿：驱动自己用接触电阻/滞回判「有没有按」，
    // 这里负责翻译成引擎的鼠标事件（按下要即时、抬手要即时、抬手后清指针状态，见 .cpp 注释）。
    // 整条指针流按 loppe::InputSource::Touch 标记。
    void touchSample(bool touched, int x, int y);

    // 或者自己管边沿时用这三个（板子有 IRQ/中断喂坐标时）
    void pointerMove(int x, int y);
    void pointerDown(int x, int y);
    void pointerUp(int x, int y);   // 在给定点抬手（click 就在这一下发出）+ 清指针状态

    // ===== 每帧一次 =====
    // ① 推进动画控件（长按连发 / 光标闪烁）→ 需要重绘时排一次 PaintOnly
    // ② 消费引擎的更新计划 → apply（只落地重排/重算样式/同步控件，并记下本帧脏区），
    //    再按返回的 PassImpact 合成一次显示列表（本类持有 Scene 与 Compositor，就是它那个槽）
    // ③ 有脏区才重画（软渲染整帧 ≈ 数十 ms，没变化就别做）
    void tick();

    // ===== 宿主自己的叠层（光标/调试数字/状态栏）=====
    // 引擎没长眼睛，看不见这些像素：画之前先标脏，才会被算进这一帧要刷的区域。
    // 坐标是逻辑像素（与 dirtyRects()/framebuffer() 同一坐标系）。
    void markDirty(int x, int y, int w, int h);
    void markFullDirty();

    // ===== 刷屏（宿主按脏区调，见头文件顶部的循环示例）=====
    bool needsFullFlush() const { return damage_.full(); }
    const std::vector<loppe::ui::Rect>& dirtyRects() const { return damage_.rects(); }
    void clearDamage() { damage_.clear(); }

    // ===== 进阶入口（要绑事件/加控件/自绘时用）=====
    loppe::Scene* scene() { return scene_.get(); }
    loppe::Compositor& compositor() { return compositor_; }
    DocumentRuntime& runtime() { return runtime_; }   // 引擎的运行时（全局命名空间）
    SoftDrawer& drawer() { return drawer_; }
    Framebuffer& framebuffer() { return fb_; }

private:
    void composeScene();
    void collectEngineDamage();
    void updateLogicalSize();
    loppe::ViewportSizeDp viewport() const;
    loppe::LogicalPointDp toLogical(int panelX, int panelY) const;

    Config cfg_{};
    bool ready_ = false;
    int  rotation_ = 0;     // 顺时针角度（cfg_.rotation 是初值，setRotation 会改它）
    int  logicalW_ = 0;     // 逻辑视口宽（旋转后）
    int  logicalH_ = 0;
    bool touchDown_ = false;    // 触摸按下状态（电平语义的另一半）
    int  touchX_ = 0;           // 最近一次触摸点（抬手也在同一点发 RELEASE）
    int  touchY_ = 0;

    Framebuffer fb_;
    SoftDrawer  drawer_;
    loppe::Renderer   renderer_{ &drawer_ };   // 声明顺序：必须在 drawer_ 之后
    loppe::Compositor compositor_;

    StyleEngine    styleEngine_;
    LayoutEngine   layoutEngine_;
    ASTBuildEngine astBuildEngine_;
    BoxModelEngine boxModelEngine_;
    DocumentRuntime runtime_;

    std::unique_ptr<loppe::Scene>           scene_;
    std::unique_ptr<loppe::EmbeddedViewCoordinator> coordinator_;
    loppe::ui::DirtyRegion                  damage_;   // 像素空间（DPR = 1）
};

} // namespace loppe_embedded
