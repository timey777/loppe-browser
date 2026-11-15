/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// dirty_region.h — 脏区累加器：把本帧要重画的若干矩形攒成「要刷的那几块」
//（可外扩、裁剪、合并；块数或面积超限时退化成「整屏」）。

#include "components/uitypes.h"
#include "loppe_export.h"

#include <cstddef>
#include <vector>

namespace loppe {
namespace ui {

class LOPPE_API DirtyRegion {
public:
    static constexpr size_t kDefaultMaxRects = 16;   // 块数默认上限：超过则整屏重画

    // ===== 策略（宿主按自己的平台定；引擎侧用默认值）=====
    void setMaxRects(size_t n);                     // 块数上限（0 视为 1）
    size_t maxRects() const;
    void setMargin(float px);                       // 每块外扩像素（圆角/取整保险），默认 0
    float margin() const;
    void setBounds(float width, float height);      // 裁剪框；任一 <= 0 = 不裁剪
    void setPixelSnap(bool on);                     // 外扩后取整（floor 左上 / ceil 右下）
    void setFullPercent(int percent);               // 面积占比 >= percent% → 整屏（需 setBounds）；<=0 = 关

    // ===== 累加 =====
    // 空矩形 / 已整屏 = 忽略；越界先裁（完全在界外直接丢）→ 外扩 margin → 取整 → 再裁；
    // 与已有块相交就并成一块；块数超上限或面积占比超阈值 → 退化成「整屏」
    void add(const Rect& r);
    void markFull();   // 整屏重画（结构重建/重排/首帧…）

    // ===== 查询 =====
    bool full() const;
    bool empty() const;
    bool hasDamage() const;
    size_t size() const;
    const std::vector<Rect>& rects() const;

    // ===== 取走/清空 =====
    void clear();                                   // 矩形 + 整屏标记都清（宿主刷完屏调）
    std::vector<Rect> takeRects();                  // 取走矩形并清整屏标记（矩形交给调用方）

private:
    void applyAreaLimit();                          // 面积占比达阈值 → 整屏
    Rect clampToBounds(const Rect& r) const;        // 裁到 setBounds 给的框（越界返回空矩形）

    size_t maxRects_ = kDefaultMaxRects;
    float  margin_ = 0.0f;
    float  boundsW_ = 0.0f;
    float  boundsH_ = 0.0f;
    bool   pixelSnap_ = false;
    int    fullPercent_ = 0;
    bool   full_ = false;
    std::vector<Rect> rects_;
};

} // namespace ui
} // namespace loppe
