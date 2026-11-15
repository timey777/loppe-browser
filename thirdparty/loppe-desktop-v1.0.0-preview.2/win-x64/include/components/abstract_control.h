/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "render/display_list.h"
#include "render/box_shadow.h"
#include "engine/core/coordinate_types.h"
#include "components/input/input_event.h"   // 事件类型（MouseButtonEvent/KeyEvent/…）
#include "components/cursor_intent.h"       // CursorIntent / cursor_prio
#include "engine/platform/platform_host.h"   // IPlatformHost（宿主可注入的平台能力）
#include "misc/stable_id.h"         // loppe::NodeUid（控件身份）
#include <string>
#include <memory>

struct ASTNode;

namespace loppe {

class Scene;
class ResolvedNode;

/** @brief 同步位掩码：本次同步包含哪些段（样式/布局）。供 OnSyncFromResolvedNode 实现方按位判断。 */
enum class ControlSync : uint32_t {
    None   = 0,
    Styles = 1u << 0,   // 样式标量：颜色/边框/圆角/阴影/可见性/padding/背景图 + 子类钩子
    Layout = 1u << 1,   // 布局产物：bounds/scroll/viewport/scroll/clipAxes + onLayoutSynced
    All    = (1u << 0) | (1u << 1),
};

constexpr ControlSync operator|(ControlSync a, ControlSync b)
{
    return static_cast<ControlSync>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
constexpr bool hasSync(ControlSync mask, ControlSync bit)
{
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(bit)) != 0;
}

/**
 * @brief 控件抽象基类
 * 
 * 所有UI控件的基类，定义了控件的通用属性和行为
 */
class LOPPE_API AbstractControl {
public:
    AbstractControl();
    virtual ~AbstractControl() = default;

    virtual bool isContainer() const;
    // 禁止拷贝
    AbstractControl(const AbstractControl&) = delete;
    AbstractControl& operator=(const AbstractControl&) = delete;

    /**
     * @brief 渲染控件到显示列表 (模板方法，处理通用逻辑如视觉平移)
     * @param displayList 显示列表
     * @param clip 祖先链路累积下传的渲染裁剪区域（本控件局部坐标系），
     *             空矩形表示无祖先裁剪约束。
     *             clip 是渲染阶段的临时参数，不属于控件，仅沿递归传递。
     */
    void render(DisplayList& displayList, const ui::Rect& clip = ui::Rect()) const;

protected:
    /**
     * @brief 实际的控件绘制逻辑，由子类实现
     * @param clip 当前有效的渲染裁剪区域，由 render() 透传。
     */
    virtual void onRender(DisplayList& displayList, const ui::Rect& clip) const = 0;

    /** @brief 子类钩子：按 mask 同步控件特有属性（hasSync(mask, ControlSync::Styles/Layout) 分段）。mask == None 时不调用；子类复用父类实现时须显式带限定名调用并透传 mask。 */
    virtual void OnSyncFromResolvedNode(const ResolvedNode& rn, ControlSync mask);

public:
    /**
     * @brief 每帧调用用于更新控件的动画/逻辑（无渲染），由 Scene 驱动
     */
    virtual void update();

    /** @brief 帧内收尾钩子：在渲染前调用，用于完成本帧内产生的内部布局更新。 */
    virtual void onFrameCommit();

    /**
     * @brief 将控件加入场景的动画更新队列，下一帧会调用 `update()`；**并请求一次重绘**
     *        （用于「我变了」：状态/外观变化的统一入口）。
     */
    virtual void invalidate();

    /** @brief 只重绘屏幕上的指定矩形（屏幕坐标），不整屏。适用于局部小变化（按键高亮、自绘光标等）。没有 Scene/ControlManager 时退化为 invalidate()。 */
    void invalidateScreenRect(const ui::Rect& screenRect);

    /** @brief 只重绘控件自身外框（滚动/拖动偏移已换算到屏幕坐标），供自绘控件局部刷新使用。 */
    void invalidateBounds();

    // 本控件外框在屏幕上的位置（bounds + 自身/祖先的 visualOffset − 祖先 scroll）。
    ui::Rect screenBounds() const;

    /**
     * @brief 只请求「下一帧继续 tick 我」——不标脏、不请求重绘。
     *
     * 适用：按时间推进但外观只在少数帧变化的控件（光标闪烁、长按连发计时…）。
     */
    void requestAnimationTick();

    /**
     * @brief 设置控件所属场景（由 Scene::addControl 调用）
     */
    virtual void setScene(Scene* scene);
    Scene* getScene() const;

    /** @brief 注入平台能力宿主（可选）；未注入且 Scene 亦无宿主时，平台能力不可用。 */
    void setPlatformHost(IPlatformHost* host);

    /**
     * @brief 设置父控件
     */
    virtual void setParent(AbstractControl* parent);
    AbstractControl* getParent() const;

    /**
     * @brief 获取控件类型名称 = **DSL 元素类型名**（`panel:foo` / `progress:bar` 里的那个词）。
     *
     * 契约：忽略大小写必须等于 `ASTNode::type`；类名与 DSL 名不同的控件必须返回 **DSL 名**
     * （ProgressBar → "progress"、RadioButton → "radio"）。要判断「运行时是什么控件」请用
     * `dynamic_cast`，不要拿它当 C++ 类名。
     */
    virtual std::string getTypeName() const = 0;

    /** @brief 将解析节点 rn 的属性按 mask（ControlSync::All / Layout）同步到控件。 */
    void applyFromResolvedNode(const ResolvedNode& rn, ControlSync mask = ControlSync::All);

    // 位置和大小
    virtual void setBounds(const ui::Rect& bounds);
    
    const ui::Rect& getBounds() const;

    // 颜色
    virtual void setFillColor(const ui::Color& color);
    const ui::Color& getFillColor() const;

    void setStrokeColor(const ui::Color& color);
    const ui::Color& getStrokeColor() const;

    // 边框
    void setStrokeWidth(const ui::Edges& width);
    const ui::Edges& getStrokeWidth() const;

    void setCornerRadius(float radius);
    float getCornerRadius() const;

    void setBackgroundImage(const std::string& path);
    const std::string& getBackgroundImage() const;

    // 阴影：传 `box-shadow` 的 CSS 值（空串 / none = 无阴影）
    void setBoxShadow(const std::string& css);
    const std::vector<BoxShadow>& getBoxShadows() const;

    // 可见性
    void setVisible(bool visible);
    bool isVisible() const;

    // 启用/禁用
    virtual void setEnabled(bool enabled);
    bool isEnabled() const;

    /** @brief 控件是否位于隔离交互作用域（如预览槽副本）内；此类控件不会出现在 findControlById 结果中。 */
    bool isInsideIsolatedScope() const;
    // ID
    void setId(const std::string& id);
    const std::string& getId() const;

    // 控件唯一标识（uid）。uidValue 为整数形态。
    void setUid(const std::string& uid);
    const std::string& getUid() const;
    void setUidValue(loppe::NodeUid uid);
    loppe::NodeUid uidValue() const;

    // class list (set from AST during control creation)
    void setClassList(const std::vector<std::string>& cls, bool forceLayout = false);
    const std::vector<std::string>& getClassList() const;
    bool hasClass(const std::string& c) const;
    void addClass(const std::string& c, bool forceLayout = false);
    void removeClass(const std::string& c, bool forceLayout = false);


    // ----------------- 事件接口 -----------------
    bool onLocalMouseMove(const ControlLocalMouseMoveEvent& e);
    bool onLocalMouseButton(const ControlLocalMouseButtonEvent& e);

    /**
     * @brief 声明光标意愿：指针在 @p point（控件局部坐标，dp）时该显示什么光标
     *
     * 每次鼠标移动，Scene 会对命中路径上的**每个**控件调一次，按 CursorIntent::priority
     * 取最高者下发 —— 与 onLocalMouseMove 的「处理即停」无关，外层控件不会被内层截断。
     * 默认不表态（返回 false）。覆写时按「控件自身几何 + 当前点」判定，不要依赖 hover 缓存。
     *
     * @param point 指针在本控件坐标系下的位置
     * @param out   type 取 Scene::CursorType 数值，priority 取 cursor_prio::kXxx
     * @return true = 本控件对这个点有明确要求
     */
    virtual bool cursorIntent(const ControlLocalPointDp& point, CursorIntent& out) const;

    ControlLocalPointDp toLocalPoint(LogicalPointDp point) const;
    bool hitTestLocal(ControlLocalPointDp point) const;
    bool hitTestPoint(LogicalPointDp point) const;

    // 键盘事件（返回 true 表示已处理）
    virtual bool onKey(const KeyEvent& e);
    // 文本输入
    virtual bool onChar(const CharEvent& e);

    bool onLocalScroll(const ControlLocalScrollEvent& e);

    // 焦点由 Scene 管理；控件在 onFocus/onBlur 中更新自身状态。
    virtual void onFocus();
    virtual void onBlur();
    bool isFocused() const;

    // 悬停状态回调（由 UIEventSystem 驱动）
    virtual void onHover(bool isHovered);

    // 激活状态回调（由 UIEventSystem 驱动，对应鼠标按下）
    virtual void onActive(bool isActive);

    virtual void onDragStart();
    void onLogicalDragMove(LogicalDragOffsetDp delta);
    virtual void onDrop();
    // Mouse capture helpers: controls can request the Scene to capture/release mouse.
    // These are convenience methods; the actual capture state is stored in Scene.
    virtual bool wantsMouseCapture() const;
    // 如果 relativeTo 不为空，则捕获期间鼠标坐标将相对于 relativeTo 的局部坐标系计算
    void captureMouse(AbstractControl* relativeTo = nullptr);
    void releaseMouse();

    // 如果控件承担可滚动或可见区域的职责，覆盖该方法以确保控件在视口中可见。
    virtual void ensureVisible();

    /**
     * @brief 布局同步完成回调。
     *
     * TreeSynchronizer 在把 bounds/viewport/scroll 同步到控件后调用。
     * 控件可在此根据新尺寸重建内部布局缓存。
     */
    virtual void onLayoutSynced();

    // 滚动相关
    void setScroll(float x, float y);
    float getScrollX() const;
    float getScrollY() const;
    
    virtual void setScrollSize(float w, float h);
    float getScrollWidth() const;
    float getScrollHeight() const;

    /** @brief 视口大小 — 滚动容器的可见窗口尺寸（仅 overflow != visible 的容器拥有，hasViewport() == true）。 */
    virtual void setViewportSize(float w, float h);
    float getViewportWidth() const;
    float getViewportHeight() const;
    bool hasViewport() const;

    /**
     * @brief 逐轴裁剪开关（来自布局的 `overflow-x` / `overflow-y`）。
     *
     * 某轴为 visible 时该轴不裁剪（内容可画到盒外、由祖先的裁剪兜底），也不出该轴滚动条。
     * 默认两轴都裁剪 —— 不写 `overflow-x/-y` 的控件行为不变。
     */
    void setClipAxes(bool clipX, bool clipY);
    bool clipsX() const;
    bool clipsY() const;

    /** @brief 节点样式里的 scrollbar-width（<= 0 = 未指定）；修改样式后下次布局生效。 */
    float styleScrollbarWidth() const;

    // 视觉位移（不触发重排）
    void setVisualOffset(float x, float y);
    float getVisualOffsetX() const;
    float getVisualOffsetY() const;

    // Padding (视觉内边距，用于内容绘制时的偏移)
    virtual void setPadding(float left, float top, float right, float bottom);
    float getPaddingLeft() const;
    float getPaddingTop() const;
    float getPaddingRight() const;
    float getPaddingBottom() const;

    // 标记图层缓存失效
    virtual void invalidateLayerCache();

protected:
    // 实际生效的平台宿主：优先宿主注入的，否则取 Scene 的（都没有 = nullptr）。
    IPlatformHost* platformHost() const;

    virtual bool handleLocalHitTest(ControlLocalPointDp point) const;
    virtual bool handleLogicalHitTest(LogicalPointDp point) const;
    virtual bool handleLocalMouseMove(const ControlLocalMouseMoveEvent& e);
    virtual bool handleLocalMouseButton(const ControlLocalMouseButtonEvent& e);
    virtual bool handleLocalScroll(const ControlLocalScrollEvent& e);
    virtual void handleLogicalDragMove(LogicalDragOffsetDp delta);

    std::string id_;
    std::vector<std::string> classes_;
    std::string uid_;                            // uidValue_ 的字符串形态（app/日志用）
    loppe::NodeUid uidValue_ = loppe::kUidNone;  // 真实身份（整数）
    ui::Rect bounds_;
    ui::Color fillColor_{0.0f, 0.0f, 0.0f, 0.0f};  // 默认透明
    ui::Color strokeColor_{0.0f, 0.0f, 0.0f, 1.0f};
    ui::Edges strokeWidth_;
    float cornerRadius_ = 0.0f;
    std::string backgroundImage_;
    std::string boxShadowCss_;
    std::vector<BoxShadow> boxShadows_;
    bool visible_ = true;
    bool enabled_ = true;
    bool focused_ = false;
    
    float scrollWidth_ = 0.0f;
    float scrollHeight_ = 0.0f;
    float viewportWidth_ = 0.0f;
    float viewportHeight_ = 0.0f;
    // 逐轴裁剪（overflow-x / overflow-y）：见 setClipAxes
    bool clipX_ = true;
    bool clipY_ = true;
    //拖动偏移
    float dragStartPosX_ = 0.0f;
    float dragStartPosY_ = 0.0f;
    // Padding
    float paddingLeft_ = 0.0f;
    float paddingTop_ = 0.0f;
    float paddingRight_ = 0.0f;
    float paddingBottom_ = 0.0f;

    // 所属 Scene（若有）
    Scene* scene_ = nullptr;
    // 宿主注入的平台能力（优先于 Scene 的，不拥有）
    IPlatformHost* platformHost_ = nullptr;
    AbstractControl* parent_ = nullptr;

    // 局部重绘优化
    mutable std::shared_ptr<DisplayList> layerDisplayList_;
    mutable bool isLayerDirty_ = true;

private:
    // 视觉位移 (不参与布局)
    float visualOffsetX_ = 0.0f;
    float visualOffsetY_ = 0.0f;

    // 滚动状态
    float scrollX_ = 0.0f;
    float scrollY_ = 0.0f;

};

} // namespace loppe