/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// popup_placement.h — 弹层落位（锚点矩形 → 视口内的最终矩形）
// ═══════════════════════════════════════════════════════════════════

#include "components/uitypes.h"   // loppe::ui::Rect
#include "loppe_export.h"

namespace loppe {

// 弹层相对锚点的首选方向。
enum class PopupSide {
    Below,   // 锚点下方（菜单/下拉的默认）
    Above,   // 锚点上方
    Right,   // 锚点右侧
    Left,    // 锚点左侧
};

// 一次弹层落位请求的参数集合。
struct LOPPE_API PopupPlacementRequest {
    ui::Rect anchor;                       // 锚点（触发控件）的屏幕矩形
    float    popupWidth  = 0.0f;           // 弹层期望尺寸
    float    popupHeight = 0.0f;
    ui::Rect viewport;                     // 可用视口；width/height <= 0 = 不限制（不翻转/不收边）
    float    gap    = 2.0f;                // 弹层与锚点的间距
    float    margin = 0.0f;                // 弹层与视口边缘的最小留白
    PopupSide preferred = PopupSide::Below;

    PopupPlacementRequest() = default;
};

// 落位策略接口：给定落位请求，返回弹层最终矩形。
class LOPPE_API IPopupPlacementStrategy {
public:
    virtual ~IPopupPlacementStrategy() = default;
    virtual ui::Rect place(const PopupPlacementRequest& request) const = 0;
};

// 默认策略：优先 request.preferred；主轴放不下则翻到对侧；最后收进视口。
class LOPPE_API FlippingPopupPlacement final : public IPopupPlacementStrategy {
public:
    ui::Rect place(const PopupPlacementRequest& request) const override;
};

// 返回进程内共享的默认落位策略实例。
LOPPE_API const IPopupPlacementStrategy& DefaultPopupPlacement();

// 按请求落位弹层；strategy 为 nullptr 时使用默认策略。
LOPPE_API ui::Rect PlacePopup(const PopupPlacementRequest& request,
                              const IPopupPlacementStrategy* strategy = nullptr);

} // namespace loppe
