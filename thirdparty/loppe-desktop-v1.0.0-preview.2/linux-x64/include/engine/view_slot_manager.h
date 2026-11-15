/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// view_slot_manager.h — 一个窗口里的视图槽集合（Stage + 各自的 Compositor / ViewCoordinator）
#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include "engine/core/coordinate_types.h"
#include "loppe_export.h"

namespace loppe {

class Stage;
class Scene;
class Compositor;
class ViewCoordinator;

/**
 * @brief 视图槽集合：一个窗口里装着几块界面时的唯一登记处。
 *
 * 一个槽 = 一块区域（`Stage`）+ 这块区域自己的 `Compositor` / `ViewCoordinator`。
 * 单文档（桌面）只注册一个槽，多文档（浏览器壳 + 标签页）注册多个；注册顺序即叠放顺序
 * （后注册的在上层），第一个注册的是「壳槽」——占满窗口的常驻界面，不参与 `detach()`。
 *
 * 提供窗口级的鼠标命中、叠画、视口下发、统一请求一帧、清指针态等操作。
 * 用法：
 *     ViewSlotManager views;
 *     views.attach(&shellStage, &shellCompositor, &shellHost);   // 第 0 槽
 *     views.attach(&tabStage, &tabCompositor, &tabHost);         // 附加槽
 *     Stage* hit = views.hitStage(x, y);                         // 鼠标按几何路由
 *     views.movePointerTo(hit);                                  // 再登记指针归属（换槽时收掉上一槽的悬停态）
 *     views.forEachHost([](ViewCoordinator* h, const Stage*) { ... });
 *     views.applyResize(windowViewport);                         // 各槽各算自己的视口
 */
class LOPPE_API ViewSlotManager {
public:
    // 注册一个视图槽；stage 已注册过则原地更新它的 compositor / host。
    void attach(Stage* stage, Compositor* compositor, ViewCoordinator* host = nullptr);

    // 注销一个视图槽（壳槽不受理）。注销的若是当前 active Stage，active 回落默认 Stage。
    void detach(Stage* stage);

    // 释放所有槽登记（不碰 Stage 生命周期）。关停时配合 clearScenes() 用。
    void clear();

    bool hasHost() const;
    std::size_t size() const;

    // 后注册的在上层，不可见跳过，没设 viewRect 视为占满窗口；全不命中返回默认 Stage。
    // 壳槽的弹层盖住该点时优先归壳（只有壳槽的弹层会浮到窗口最上层）。
    Stage* hitStage(float x, float y) const;

    // 更新「指针当前在哪一槽」；从别的槽转过来时，先把上一槽的悬停/按下态收回。
    // 每次投递指针事件（移动 / 按下）前调用一次，传本次事件命中的槽。
    void movePointerTo(Stage* stage);

    // 遍历所有注册了 host 的槽（fn 收到 host 与该槽的 Stage）。
    void forEachHost(const std::function<void(ViewCoordinator* host, const Stage* stage)>& fn) const;

    // 帧末合成：把指定场景的当前控件状态生成成 display list（scenes 为空集合等价于 composeAll）。
    void compose(const std::vector<Scene*>& scenes);

    // 帧末合成全部可见槽的画面（首帧 / 窗口级保底帧用）。
    void composeAll();

    // 按注册顺序叠画所有可见槽：先各槽正文（附加槽的弹层紧跟自己、裁在其 viewRect 内），
    // 最后只把壳槽（第 0 槽）的弹层画到窗口最上层。
    void paintAll();

    // 按槽下发视口：每槽视口 = 它自己的 viewRect 宽高（没设 viewRect = windowViewport），
    // 只对「视口真的变了」的槽调 handleResize。返回是否有槽被重排。
    bool applyResize(const ViewportSizeDp& windowViewport);

    // 只重算某一个槽：视口取它自己的 viewRect，判等照旧。
    // 用于「窗口尺寸没变、但这个槽的内容区变了」（多标签里页面矩形跟着壳布局动）；
    // 该槽没有 viewRect（占满窗口）或没登记 host 时什么都不做。返回是否真的重排了。
    bool applySlotResize(Stage* stage);

    // 每槽自己的调度器各发一次 requestPaint（窗口级「至少出一帧」的落点）。
    void requestPaintAll();

    // 所有槽当前场景清悬停/按下状态（鼠标离开窗口、窗口失焦）。
    void clearPointerStates();

    // 所有槽当前场景关闭弹层（菜单/下拉/浮窗）。
    void dismissAllOverlays();

    // 所有槽的 Stage 提前释放场景（有序关停：Scene 析构会带走它持有的 runtime/解析树）。
    void clearScenes();

private:
    struct Slot {
        Stage* stage = nullptr;
        Compositor* compositor = nullptr;
        ViewCoordinator* host = nullptr;
        ViewportSizeDp appliedViewport{ { 0.0f }, { 0.0f } };
    };

    // 壳槽（占满窗口的常驻界面）：永远是最先 attach 的那个，即 slots_.front()。
    // 它是唯一允许把弹层提升到窗口最上层的槽 —— 其余槽的弹层只许留在自己矩形内。
    // 集中在壳槽判断上，避免「第 0 槽」这个下标在 paintAll / hitStage 里各自硬编码。
    Stage* shellStage() const { return slots_.empty() ? nullptr : slots_.front().stage; }
    bool isShellSlot(std::size_t index) const { return index == 0; }

    // 附加槽的弹层：就地画、裁在自己的 viewRect 内，不提升到窗口层。
    void paintOverlayInPlace(const Slot& slot);

    std::vector<Slot> slots_;

    // 上一次指针事件落到的槽（移动/按下共同维护）。指针换槽时上一槽要收回指针态；
    // 槽被 detach 或统一清指针态时置空。
    Stage* pointerStage_ = nullptr;
};

} // namespace loppe
