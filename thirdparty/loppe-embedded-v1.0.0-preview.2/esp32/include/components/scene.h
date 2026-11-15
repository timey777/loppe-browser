/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "components/abstract_control.h"
#include "components/cursor_intent.h"
#include "components/input/input_event.h"
#include "components/container_control.h"
#include <algorithm>
#include "render/display_list.h"
#include "components/overlay_event_root.h"
#include <memory>
#include <vector>
#include <functional>
#include "engine/document_runtime.h"
#include "engine/event/ui_event_system.h"
#include <unordered_map>
#include <unordered_set>
#include <engine/core/resolved_tree.h>
#include "engine/core/resolved_node.h"
#include "engine/core/coordinate_types.h"
#include "engine/core/input_source.h"
#include <engine/capability/drag_manager.h>
#include <engine/capability/dismiss_manager.h>
#include <engine/core/resolved_tree_view.h>
class UpdateScheduler;
namespace loppe {

class ControlManager;
class IInputCaptureTarget;
class InteractionScope;
class InteractionScopeRegistry;
class SoftKeyboard;

/**
 * @brief 场景类 - 管理所有UI控件
 */
class LOPPE_API Scene {
    friend class ControlManager;
public:
    Scene();
    explicit Scene(::DocumentRuntime& runtime);
    virtual ~Scene();

    // 显式绑定/解绑文档（纸↔黑板关联）：attach 后场景引用文档的树/引擎；
    // detach 清空一切指向文档的引用，场景可独立存活。文档析构时也会通知关联场景 detach。
    void attach(::DocumentRuntime& runtime);
    void detach();
    bool isAttachedToDocument() const;


    virtual bool setup();

    /** @brief AST 更新且控件树就绪后的回调（非每帧；一帧内可能多次，实现需幂等）。回调中对控件的修改会在本帧画面生效。 */
    virtual void onControlsReady();

    ::DocumentRuntime* getDocumentRuntime() const;

    // ResolvedTree管理（Scene不拥有ResolvedTree，仅持有引用）
    ResolvedTreeView* getResolvedTreeView();
    ResolvedTreeView* getResolvedTree();

    // 便捷获取文本测量器（可能为空）
    ITextMeasurer* getTextMeasurer() const;

    /**
     * @brief 渲染场景中的所有控件
     */
    virtual void render(DisplayList& displayList);

    /**
     * @brief 只渲染主控件树（不含覆盖层与软键盘）
     */
    virtual void renderContent(DisplayList& displayList);

    /**
     * @brief 只渲染覆盖层（菜单/下拉/浮窗）与软键盘；窗口级最后一层的绘制内容
     */
    virtual void renderOverlay(DisplayList& displayList);

    /**
     * @brief 本场景的覆盖层是否盖住该点（场景坐标，与鼠标派发同一口径）
     */
    bool overlayHitTest(const LogicalPointDp& point) const;

    ControlManager* getControlManager() const;

    // 事件分发与焦点管理（高层 API）
    // 返回是否被处理
    bool onMouseButton(const MouseButtonEvent& e);
    void onMouseMove(const MouseMoveEvent& e);
    void onScroll(const ScrollEvent& e);
    virtual void onKey(const KeyEvent& e);
    void onChar(const CharEvent& e);

    // ── 输入方式（触摸 / 鼠标）──────────────────────────────────────────
    // 当前来源（粘性）：事件结构体里没填 source 时按它算，填了（非 Unspecified）则先更新再派发。
    void setInputSource(InputSource src);
    InputSource inputSource() const;

    // 懒创建的内置软键盘；焦点进出输入框时自动弹出/收起。
    SoftKeyboard* softKeyboard();
    void setSoftKeyboardViewport(float w, float h);

    // 内置软键盘总开关（默认开）：关掉后本场景不显示、也不自动弹出；可随时再开。
    // 宿主用系统输入法（如安卓）时，建好场景后设为 false。
    void setSoftKeyboardEnabled(bool on);
    bool softKeyboardEnabled() const;

    // 绑定便捷 API
    void onClick(const std::string& selector, UIEventSystem::EventCallback cb);
    void onInput(const std::string& selector, UIEventSystem::EventCallback cb);
    // 滚轮事件绑定（指针事件 onScroll 的便捷绑定重载）。
    void onScroll(const std::string& selector, UIEventSystem::EventCallback cb);
    // 控件内值变化（如 ComboBox 选中项变化 → Change）。
    void onChange(const std::string& selector, UIEventSystem::EventCallback cb);

    // 本场景所属文档的调度器（经 runtime 反查）。未 attach 到 DocumentRuntime 时返回 nullptr
    // —— 调用方需判空；此时更新请求不会被任何一帧消费。
    UpdateScheduler* scheduler() const;

    // 访问 Scene 中的 UIEventSystem
    UIEventSystem* getUIEventSystem();

    // 节点被移除时的清理工作
    void notifyNodeRemoved(ResolvedNode* node);

    // Debug accessor: return last hit-test path (ResolvedNode* path)
    const std::vector<ResolvedNode*>& getDebugHitPath() const;


    /**
     * @brief 将 Scene 中运行时可持久化的数据（例如 Input 的 value）写回到 AST
     * @param root 全局 AST 的根节点（用于按 id 查找并写回）
     * @param overwrite 如果为 true 则强制覆盖 AST 中已存在的 value，否则仅在 AST 中不存在 value 时写回
     */
    void syncToAST(std::shared_ptr<ASTNode> root, bool overwrite = false);

    // 清除与指针相关的运行时状态（悬停/激活），例如当窗口失去焦点或鼠标在窗外释放时调用
    void clearPointerState();

    // 只收回指针态（悬停/按下/捕获/拖拽），焦点与光标都不动。
    // 用于指针移到同一窗口里的另一份文档（另一块 Stage）时，由窗口层调用。
    void releasePointerState();

    // 重新评估一次 hover（不依赖鼠标移动）：控件 setEnabled(true) 之后调用。
    void reapplyHover();

    // 简单的鼠标捕获（仅在窗口内生效）：当一个控件需要接收全局鼠标事件时，调用 setMouseCapture
    void setMouseCapture(ResolvedNode* node, ResolvedNode* relativeTo = nullptr);
    void setMouseCaptureControl(AbstractControl* ctrl, AbstractControl* relativeTo = nullptr);
    void clearMouseCapture();
    ResolvedNode* getMouseCapture() const;

    // 弹层「外部点击/失焦 dismiss」（菜单/下拉/浮窗）：组件打开时 add、关闭时 remove；
    // Scene 在鼠标按下与窗口失焦时自动调用。
    DismissManager& getDismissManager();

    // Cursor types (mirror of Window::CursorType).
    enum CursorType {
        Cursor_Arrow = 0,
        Cursor_Hand = 1,
        Cursor_IBeam = 2,
        Cursor_ResizeHorizontal = 3,
        Cursor_ResizeVertical = 4
    };

    /**
     * @brief 登记一个光标意愿，本次鼠标移动末尾统一裁决、只下发一次
     *
     * 每次 onMouseMove 开始时清空所有意愿，控件在 cursorIntent() 或事件回调里按
     * @p priority 登记（取 cursor_prio::kXxx）；同优先级后登记者胜。不要在移动回调之外
     * 调用 —— 下次移动就会按新的命中结果重算。需要「一直保持」的手势请在 cursorIntent()
     * 里按当前点表态。
     */
    void requestCursor(int cursorType, int priority = cursor_prio::kArrow);

    // 本次移动的光标裁决结果（供宿主/诊断查询）；未裁决时为 Cursor_Arrow。
    int getPendingCursor() const;
    // 当前已下发给平台的光标类型。
    int getActiveCursor() const;

    // Apply the pending cursor state to the engine context if it differs from the active state.
    void flushCursor();

    // 进程级平台能力宿主（可能为空）。
    IPlatformHost* getPlatformHost() const;

private:
    void resetPressedControls();
    bool onMouseButtonImpl(const MouseButtonEvent& e);

    std::unique_ptr<ControlManager> controlManager_;
    // 鼠标位置缓存
    LogicalPointDp lastMousePoint_;
    // 输入方式（触摸 / 鼠标）：宿主设，或由事件自带的 source 更新
    InputSource inputSource_ = InputSource::Mouse;
    std::unique_ptr<SoftKeyboard> softKeyboard_;   // 懒创建（触摸屏文本输入）
    bool softKeyboardEnabled_ = true;              // 宿主可关（用系统输入法时）
    float kbViewportW_ = 0.f, kbViewportH_ = 0.f;  // 最近一次已知的 viewport（建键盘时用）
    ResolvedNode* lastHitNode_ = nullptr;

    InteractionScope* lastInteractionScope_ = nullptr;
    const InteractionScopeRegistry* interactionScopes() const;
    UIEventSystem* focusEventSystem();
    // 焦点变化（UIEventSystem 的观察者）：焦点进出输入框时驱动软键盘自动弹出/收起
    void onFocusChanged(ResolvedNode* focusNode);
    // 统一输入捕获目标（可由 ResolvedNode 或 AbstractControl 适配实现）
    std::unique_ptr<IInputCaptureTarget> mouseCaptureTarget_;
    ::DocumentRuntime* runtime_ = nullptr;
    // 文档销毁通知订阅 ID（0=未订阅；attach 时订阅，detach 时取消）
    int destroySubId_ = 0;

    // 解析树（非拥有，仅引用）
    ResolvedTreeView resolvedTree_;

    // Scene 自持有 UIEventSystem
    UIEventSystem uiEventSystem_; 
    DragManager   dragManager_;
    // 弹层「外部点击/失焦 dismiss」管理器（菜单等浮层）
    DismissManager dismissManager_;

    // 保存最近一次命中的控件路径（用于调试绘制），以 ResolvedNode* 表示
    std::vector<ResolvedNode*> debugHitPath_;

    int pendingCursor_ = Cursor_Arrow;
    // 本次移动已登记的最高优先级；-1 = 还没有任何控件表态
    int pendingCursorPri_ = -1;
    int activeCursor_ = Cursor_Arrow;
};

} // namespace loppe