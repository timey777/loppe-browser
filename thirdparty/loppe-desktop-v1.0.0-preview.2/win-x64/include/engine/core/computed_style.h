/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "render/display_list.h"
#include "node_flags.h"
#include <string>
#include <array>
#include <bitset>
#include <vector>

namespace loppe {

enum class Anchor {
    None,
    TopLeft,
    Center,
    TopRight,
    BottomLeft,
    BottomRight
};

Anchor stringToAnchor(const std::string &str);

enum class StyleProp : uint32_t {
    BackgroundColor = 0,
    TextColor,
    BorderColor,
    BorderWidth,
    BorderRadius,
    FontSize,
    LineHeight,
    FontFamily,
    FontWeight,
    FontStyle,
    TextAlign,
    Width,
    Height,
    MinWidth,
    MinHeight,
    MaxWidth,
    MaxHeight,
    Overflow,
    // 按轴覆盖（`overflow-x` / `overflow-y`）：只影响该轴的裁剪 / 滚动条 / 溢出传播
    OverflowX,
    OverflowY,
    ScrollbarWidth,
    ScrollbarGutter,
    Padding,
    Margin,
    MarginAuto,
    Opacity,
    Visible,
    Overlay,
    Anchor,
    OffsetX,
    OffsetY,
    BoxShadow,
    BackgroundImage,
    ZIndex,
    Count
};

struct ComputedStyle {
    ui::Color backgroundColor{0, 0, 0, 0};
    ui::Color textColor{0, 0, 0, 1};
    ui::Color borderColor{0, 0, 0, 0};
    ui::Edges borderWidth = {0.0f, 0.0f, 0.0f, 0.0f};
    UIUnit borderRadius = UIUnit::px(0.0f);
    float fontSize = 14.0f;
    float lineHeight = 0.0f;
    std::string fontFamily;
    FontWeight fontWeight = FontWeight::Normal;
    FontStyle fontStyle = FontStyle::Normal;
    TextAlign textAlign = TextAlign::Left;
    LayoutUnit width = LayoutUnit::autoUnit();
    LayoutUnit height = LayoutUnit::autoUnit();
    LayoutUnit minWidth = LayoutUnit::autoUnit();
    LayoutUnit minHeight = LayoutUnit::autoUnit();
    LayoutUnit maxWidth = LayoutUnit::autoUnit();
    LayoutUnit maxHeight = LayoutUnit::autoUnit();
    // `overflow` 简写：设置它等于同时设置两个轴。
    Overflow overflow = Overflow::Auto;
    // 逐轴生效值：只有显式写了 `overflow-x` / `overflow-y` 才会与 overflow 不同。
    Overflow overflowX = Overflow::Auto;
    Overflow overflowY = Overflow::Auto;
    // 本节点滚动条粗细（px）。<= 0 = 未指定，回退到默认。
    float scrollbarWidth = 0.0f;
    // `scrollbar-gutter: stable`：滚动轴恒留一条滚动条宽的槽，内容视口始终扣掉它，
    // 与内容是否溢出、滚动条是否出现无关（不写 = 按需占位，滚动条出现才扣）。
    bool scrollbarGutterStable = false;
    ui::Edges padding = {0, 0, 0, 0};
    ui::Edges margin = {0, 0, 0, 0};
    std::array<bool, 4> marginAuto = {false, false, false, false};

    float opacity = 1.0f;
    bool visible = true;
    bool overlay = false;
    Anchor anchor = Anchor::None;
    float offsetX = 0, offsetY = 0;
    std::string boxShadow = "";
    std::string backgroundImage = "";
    // 同级叠放顺序：越大越靠上（同值时按声明顺序，后声明的在上）。
    int zIndex = 0;
};

/**
 * @brief 表示一组样式声明的增量
 */
struct StyleDeclaration : public ComputedStyle {
    std::bitset<static_cast<size_t>(StyleProp::Count)> hasValue;

    bool has(StyleProp prop) const;
    void set(StyleProp prop);
    void clear(StyleProp prop);
};

// 保持兼容性别名
using StyleDelta = StyleDeclaration;

/**
 * @brief 封装特定状态下的样式规则逻辑
 */
struct StateRule {
    StyleDeclaration delta;
    bool hasEffect = false;
    bool affectsLayout = false;
    bool affectsPaint = false;
};

/**
 * @brief 封装样式层叠和物理顺序逻辑
 */
struct StyleCascader {
    struct Entry {
        loppe::UIState requiredState = loppe::UIState::None;
        loppe::StyleDeclaration delta;
        bool affectsLayout = false;
        bool affectsPaint = true;
        int order = 0;
    };

    // 所有按源码顺序匹配的规则
    std::vector<Entry> orderedRules;

    // 基础样式（UIState::None）中各属性对应的最大 Order
    std::array<int, static_cast<size_t>(StyleProp::Count)> basePropertyOrders;

    StyleCascader();

    void updateBaseOrder(StyleProp prop, int order);

    bool canOverride(StyleProp prop, int currentOrder) const;
};

// 预编译后的样式规则集合。
struct CompiledStyleRules {
    loppe::ComputedStyle base;		 // rules with UIState::None merged

    // 状态规则索引。
    enum StateIndex {
        Hover = 0,
        Active,
        Focus,
        Dragging,
        Disabled,
        Count
    };
    
    std::array<StateRule, StateIndex::Count> stateRules;

    // 层叠逻辑封装
    StyleCascader cascader;

    // 是否有状态增量会影响布局/绘画。
    bool hasStatefulLayout = false;
    bool hasStatefulPaint = false;

    CompiledStyleRules() = default;
};

} // namespace loppe