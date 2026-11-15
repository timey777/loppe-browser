/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
#include <string>
#include <array>
#include "render/display_list.h"
#include "node_flags.h"
#include "computed_style.h" // For LayoutUnit, ui::Edges etc.

namespace loppe {

    /**
     * @brief 属性提供者模型：读取节点的盒模型 / 布局上下文 / 文字与内容测量属性
     */
    class IPropertyProvider {
    public:
        virtual ~IPropertyProvider() = default;

        // --- 盒模型核心几何属性 ---
        virtual LayoutUnit getWidth() const = 0;
        virtual LayoutUnit getHeight() const = 0;
        virtual LayoutUnit getMinWidth() const = 0;
        virtual LayoutUnit getMinHeight() const = 0;
        virtual LayoutUnit getMaxWidth() const = 0;
        virtual LayoutUnit getMaxHeight() const = 0;
        virtual ui::Edges getPadding() const = 0;
        virtual ui::Edges getMargin() const = 0;
        virtual std::array<bool, 4> getMarginAuto() const = 0;
        virtual ui::Edges getBorderWidth() const = 0;

        // --- 布局上下文属性 ---
        virtual FormattingContext getDisplay() const = 0;
        virtual Position getPosition() const = 0;

        // --- 文字与内容测量属性 ---
        virtual float getFontSize() const = 0;
        virtual float getLineHeight() const = 0;
        virtual std::string getFontFamily() const = 0;
        virtual FontWeight getFontWeight() const = 0;
        virtual FontStyle getFontStyle() const = 0;
        virtual TextAlign getTextAlign() const = 0;
        virtual Overflow getOverflow() const = 0;
        // 逐轴生效值（`overflow` 简写已同步进来；显式 `overflow-x`/`overflow-y` 覆盖对应轴）
        virtual Overflow getOverflowX() const = 0;
        virtual Overflow getOverflowY() const = 0;
        // 本节点滚动条粗细（<= 0 = 未指定，布局按默认滚动条预留）
        virtual float getScrollbarWidth() const = 0;
        // `scrollbar-gutter: stable`：true = 滚动轴恒留槽（视口始终扣掉滚动条宽）
        virtual bool getScrollbarGutterStable() const = 0;

        // --- 悬浮/绝对定位属性 ---
        virtual bool isOverlay() const = 0;
        virtual Anchor getAnchor() const = 0;
        virtual float getOffsetX() const = 0;
        virtual float getOffsetY() const = 0;

        // --- Flex 布局属性 ---
        virtual FlexDirection getFlexDirection() const = 0;
        virtual AlignItems getAlignItems() const = 0;
        virtual AlignSelf getAlignSelf() const = 0;
        virtual JustifyContent getJustifyContent() const = 0;
        virtual FlexWrap getFlexWrap() const = 0;
        virtual float getFlexGrow() const = 0;
        virtual float getFlexShrink() const = 0;
        virtual float getGap() const = 0;

        // --- Grid 布局属性 ---
        virtual std::string getGridTemplateColumns() const = 0;
        virtual std::string getGridTemplateRows() const = 0;
    };


} // namespace loppe