/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "abstract_control.h"
#include <memory>
#include <vector>

namespace loppe {

// Forward declare AbstractControl usage; ScrollbarControl works with any control exposing scroll API
class AbstractControl;

class ScrollbarControl : public AbstractControl {
public:
    // 内置基准粗细（dp）。未调用 setDefaultThickness() 时取此值。
    static constexpr float kScrollbarThickness = 8.0f;
    // 滑块最短长度（dp）
    static constexpr float kMinThumbSize = 32.0f;

    // 全局默认粗细（dp，非 sp）；<= 0 或 NaN 恢复 kScrollbarThickness；
    // 须在创建界面之前调用。
    static void setDefaultThickness(float dp);
    static float defaultThickness();

    enum class Orientation { Vertical, Horizontal };

protected:
    // 仅 ControlFactory（friend）可创建实例
    ScrollbarControl(Orientation o);
    friend class ControlFactory;

    // 子类/工厂定制点：覆写即可换粗细、最短长度、配色。
    virtual float barThickness() const;
    virtual float minThumbSize() const;
    // 常态滑块颜色
    virtual ui::Color thumbColor() const;
    // 指针停在滑块上时的滑块颜色；默认 = thumbColor() 加浓一档（EmphasizeColor）。
    virtual ui::Color thumbHoverColor() const;

    // 悬浮强调：滑块颜色在悬浮时加浓一档。
    static ui::Color EmphasizeColor(const ui::Color& c);

public:
    ~ScrollbarControl() override = default;
    std::string getTypeName() const override;

    // 实际粗细：实例覆盖（>0） > 节点样式 scrollbar-width（>0） > barThickness()。
    float getBarThickness() const;
    // 实例级粗细覆盖（dp）；0 = 清除覆盖。
    void setBarThickness(float t);

    // 轨道起点/长度（parent-local，沿本实例的轴）。不调用 = 默认轨道：
    // 起点 = 父控件 padding box 起点，长度 = 视口尺寸；
    // 只给 start（length <= 0）= 只挪起点，长度仍用视口尺寸。
    void setTrack(float start, float length = 0.0f);

    // 悬浮状态 = 指针停在滑块（thumb）上。拖拽期间应一直保持 true；
    // 指针离开控件时调用方置 false。
    void setHovered(bool hovered);
    bool isHovered() const;
    // 泳道悬浮 = 指针落在整条泳道内（含滑块以外的空白段）。
    bool isLaneHovered() const;

    // 轨道标记：start/end 为内容比例（0 = 内容顶部，1 = 内容底）；
    // end <= start 时按最小高度绘制；setMarkers() 整份替换，空列表 = 清空。
    struct Marker {
        float start = 0.0f;
        float end = 0.0f;
        ui::Color color;
    };
    void setMarkers(std::vector<Marker> markers);
    const std::vector<Marker>& markers() const;

    // 命中测试：parent-local 点是否落在滚动条泳道（轨道）内。
    bool containsPoint(ParentLocalPointDp point) const;
    // 命中测试：parent-local 点是否落在滑块（thumb）上。
    bool containsThumb(ParentLocalPointDp point) const;

protected:
    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

public:
    bool onParentLocalMouseMove(const ParentLocalMouseMoveEvent& e);
    bool onParentLocalMouseButton(const ParentLocalMouseButtonEvent& e);
    void onActive(bool isActive) override;

protected:
    virtual bool handleParentLocalMouseMove(const ParentLocalMouseMoveEvent& e);
    virtual bool handleParentLocalMouseButton(const ParentLocalMouseButtonEvent& e);

private:
    float viewportSize() const;
    float trackStart() const;
    float trackLen() const;
    ui::Rect thumbRect() const;
    void requestThumbPaint() const;

    Orientation orient_;
    bool dragging_ = false;
    bool hovered_ = false;
    bool laneHovered_ = false;
    float customThickness_ = 0.0f;
    bool hasTrack_ = false;
    float trackStart_ = 0.0f;
    float trackLength_ = 0.0f;
    float dragStartPos_ = 0.0f;
    float dragStartScroll_ = 0.0f;
    std::vector<Marker> markers_;
    static constexpr float kMarkerMinSize = 3.0f;
};

} // namespace loppe
