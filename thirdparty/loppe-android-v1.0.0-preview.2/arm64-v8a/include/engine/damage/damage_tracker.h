/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, and distribution is prohibited.
 */

#pragma once

#include "components/uitypes.h"
#include "engine/damage/dirty_region.h"
#include "engine/document_update_plan.h"
#include "loppe_export.h"

#include <vector>

namespace loppe {

class ResolvedNode;

// 脏区记录器：记录本视图本帧需要重画的屏幕矩形区域。
//
// 记账单位为屏幕坐标矩形；tracking=false（平台无局部刷新能力）时全部记账为空操作，
// 行为等同每帧整屏重画。
//
// 用法 A —— 已知矩形，直接记账：
//     tracker.markFull();                    // 整屏（重建/重排这类说不清范围的）
//     tracker.markRect(r);                   // 已知矩形（动画、软键盘按键高亮…）
//     tracker.markNode(node);                // 节点当前外框（含 box-shadow 外沿）
//     tracker.markNodeBox(node, oldBox);     // 同上，但外框由调用方给（试排已改写 layout）
//
// 用法 B —— 手里是一帧的更新计划，整份喂进来：
//     tracker.absorbPlan(plan);              // 执行前：kind → 基线 + 动画矩形 + 整屏兜底 + 旧外框
//     ... 执行 ...
//     tracker.absorbExecuted(plan);          // 执行后：盒子可能变了的帧补新外框
//
// 取走（两种用法都一样，每帧末一次）：
//     const auto upd = tracker.take();       // full + rects 一次取走并清空
//     if (upd.full) repaintAll(); else repaint(upd.rects);
//
// 前置条件：每帧恰好调用一次 take() 取走脏区。
class LOPPE_API DamageTracker {
public:
    // 一次取走的脏区：full = 整屏重画；否则只刷 rects 那几块
    struct Update {
        bool full = false;
        std::vector<ui::Rect> rects;
    };

    DamageTracker();

    // 没有局部刷新能力的平台关掉即可（所有记账变空操作，零开销）
    void setTracking(bool on);
    bool tracking() const;

    // 是否欠一次整屏（tracking 关闭时恒 false）
    bool isFull() const;

    // ===== 记账 =====

    void markFull();                                   // 整屏
    void markRect(const ui::Rect& r);                  // 逻辑矩形（自动合并/超限退化整屏）
    void markNode(ResolvedNode* node);                 // 节点当前边框盒（含 box-shadow 外沿）
    // 同上，但边框盒由调用方提供（当前布局已被更新、调用方持有更新前快照时使用）。
    void markNodeBox(ResolvedNode* node, const ui::Rect& borderBox);

    // ===== 一整帧：把更新计划翻译成记账 =====

    // 执行更新计划**前**调用一次：把计划翻译成脏区记账；重复调用安全（累加合并）。
    void absorbPlan(const DocumentUpdatePlan& plan);

    // 执行更新计划**后**调用一次：补采更新后节点的新外框（仅局部样式更新、重排类计划需要）。
    void absorbExecuted(const DocumentUpdatePlan& plan);

    // 采样节点当前边框盒（含 box-shadow 外沿）。
    void sample(ResolvedNode* node);
    void sample(const std::vector<ResolvedNode*>& nodes);
    // 采样调用方提供的边框盒快照（节点当前布局已被更新时使用）。
    void sampleBox(ResolvedNode* node, const ui::Rect& borderBox);

    // ===== 取走 =====

    // 一次取走并清空（推荐入口）；每帧恰好一次
    Update take();
    // 兼容入口：只取走矩形并清空记录（整屏标记不动，见 needsFullRedraw 的老用法）
    std::vector<ui::Rect> takeRects();

    // 只读视图（引擎内部核对用；不要用它记账）
    const ui::DirtyRegion& region() const;

private:
    ui::DirtyRegion region_;
    bool tracking_ = true;
};

} // namespace loppe
