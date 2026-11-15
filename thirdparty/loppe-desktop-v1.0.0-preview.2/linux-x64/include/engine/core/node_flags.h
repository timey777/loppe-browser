/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <cmath>
#include "components/uitypes.h"

namespace loppe {

enum class FormattingContext {
    None,
    Inline,
    Block,
    Flex,
    Grid
};

static constexpr float AUTO_SENTINEL = -1.0f;
static constexpr float UNBOUNDED = std::numeric_limits<float>::infinity();

bool isAuto(float v);
bool isDefinite(float v);

enum class Unit : uint8_t {
    Px,
    Dp,
    Sp,
    Percent,
    Auto
};

struct LayoutUnit {
    float value = 0.0f;
    Unit unit = Unit::Auto;

    bool isAuto() const;
    bool isPercent() const;
    bool isPx() const;
    bool isDp() const;
    bool isSp() const;
    bool isAbsoluteLength() const;

    static LayoutUnit px(float v);
    static LayoutUnit dp(float v);
    static LayoutUnit sp(float v);
    static LayoutUnit percent(float v);
    static LayoutUnit autoUnit();

    static LayoutUnit fromString(const std::string& s);

    float resolve(float parentSize, bool parentIsDefinite = true) const;

    float resolveToPx(float parentSize, bool parentIsDefinite = true) const;
};

// UIUnit is a simpler alternative for properties like border-radius that primary use pixels
struct UIUnit {
    float value = 0.0f;

    static UIUnit px(float v);
    static UIUnit fromString(const std::string& s);
    float toPx() const;
};

bool tryParseAbsoluteLength(const std::string& s, float& outValue);
bool tryParseEdgeLengthShorthand(const std::string& s, ui::Edges& out,
                                 std::array<bool, 4>* outAuto = nullptr);

enum class FlexDirection : uint8_t { Row, Column, RowReverse, ColumnReverse };
enum class AlignItems : uint8_t { Stretch, FlexStart, FlexEnd, Center, Baseline };
enum class AlignSelf : uint8_t { Auto, Stretch, FlexStart, FlexEnd, Center, Baseline };
enum class JustifyContent : uint8_t { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly };
enum class FlexWrap : uint8_t { NoWrap, Wrap, WrapReverse };
enum class Overflow : uint8_t { Visible, Hidden, Scroll, Auto };
enum class Position : uint8_t { Static, Relative, Absolute, Fixed };

enum class FontWeight : uint8_t {
    Normal,
    Bold,
    W100, W200, W300, W400, W500, W600, W700, W800, W900
};

enum class FontStyle : uint8_t {
    Normal,
    Italic,
    Oblique
};

enum class TextAlign : uint8_t {
    Left,
    Center,
    Right,
    Justify
};

FontWeight fontWeightFromString(const std::string& s);
FontStyle fontStyleFromString(const std::string& s);
TextAlign textAlignFromString(const std::string& s);
Overflow overflowFromString(const std::string& s);
FormattingContext displayStringToEnum(const std::string& d);
Position positionStringToEnum(const std::string& s);
FlexDirection flexDirectionStringToEnum(const std::string& s);
AlignItems alignItemsStringToEnum(const std::string& s);
AlignSelf alignSelfStringToEnum(const std::string& s);
JustifyContent justifyContentStringToEnum(const std::string& s);
FlexWrap flexWrapStringToEnum(const std::string& s);

// ================= UI 状态 =================
enum class UIState : uint8_t {
    None = 0,
    Hover = 1 << 0,
    Active = 1 << 1,
    Focus = 1 << 2,
    Dragging = 1 << 3,
    Disabled = 1 << 4
};

UIState operator|(UIState a, UIState b);

UIState operator&(UIState a, UIState b);

bool hasUIState(UIState s, UIState test);

// ================= 脏标记 =================
enum class DirtyFlag : uint32_t {
    None = 0,
    Layout = 1 << 0,
    Style = 1 << 1,
    Paint = 1 << 2,
    ChildrenLayout = 1 << 3,
    Removed = 1 << 4,
    All = 0xF   // Layout | Style | Paint | ChildrenLayout
};

inline constexpr DirtyFlag operator|(DirtyFlag a, DirtyFlag b) {
    return static_cast<DirtyFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

DirtyFlag operator&(DirtyFlag a, DirtyFlag b);

DirtyFlag& operator|=(DirtyFlag& a, DirtyFlag b);

bool hasDirtyFlag(DirtyFlag flags, DirtyFlag check);

// 调试辅助：将 DirtyFlag 转换为可读字符串
std::string dirtyFlagToString(DirtyFlag flags);

} // namespace loppe