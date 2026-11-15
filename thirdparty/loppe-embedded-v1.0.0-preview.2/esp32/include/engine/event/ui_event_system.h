/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
#include <vector>
#include <functional>
#include <string>
#include <map>
#include <unordered_set>
#include "engine/core/resolved_tree.h"
#include "engine/core/resolved_node.h"
#include "components/input/input_event.h"   // mouse_button::kXxx（UIEvent::button 的取值）
namespace loppe {
class AbstractControl;

class LOPPE_API UIEventSystem {
public:
    UIEventSystem() = default;
    ~UIEventSystem() = default;

    // 焦点变化观察者（可选）：每次焦点真正切换后调用一次（含清空焦点、焦点节点被移除）。
    using FocusObserver = std::function<void(ResolvedNode* newFocus)>;
    void setFocusObserver(FocusObserver cb);

    void updateHover(ResolvedNode* newHover);
    void updateActive(ResolvedNode* newActive);
    void updateFocus(ResolvedNode* newFocus);
    ResolvedNode* getFocusNode();
    // 移除节点时清空相关引用
    void notifyNodeRemoved(ResolvedNode* node);
    // 清空 hover/active/focus/pressed 全部状态（解除文档关联时调用）
    void clearAllStates();
    // 清空指针相关状态（hover/active/pressed），**保留焦点**。
    // 指针离开窗口 / 窗外释放时用；与 clearAllStates() 的区别就是不碰 focus。
    void clearPointerStates();
    // 返回当前 hover 路径的最深节点（如果没有 hover 则返回 nullptr）
    ResolvedNode* getHoverNode() const;

    // 本次手势中左键按下过的控件集合。
    std::unordered_set<AbstractControl*>& pressedControls();
    void clearPressedControls();
    void erasePressedControl(AbstractControl* ctrl);

    // ----------------- Event binding API -----------------
    // Click = **主键（左键）**按下后在同一个控件上抬起；右键 / 中键不产生 Click
    // （要处理它们就绑 MouseDown/MouseUp，按 UIEvent::button 判键）。
    enum class UIEventType {
        Click,
        MouseDown,
        MouseUp,
        Input,
        Change,
        Scroll,
        Focus,
        Blur,
        KeyDown,
        KeyUp
    };

    struct UIEvent {
        UIEventType type;
        // 当前派发到的控件（= DOM 的 event.currentTarget）
        loppe::AbstractControl* target = nullptr;
        // 命中的**最深**控件（= DOM 的 event.target）：子控件被点击时，祖先处理器也能看出「点的是谁」。
        loppe::AbstractControl* deepestTarget = nullptr;
        // 处理器置 true → 不再向祖先传播（= DOM 的 stopPropagation；同一节点上排在后面的处理器
        // 也不再执行，即 stopImmediatePropagation 语义）。
        bool stopPropagation = false;

        // optional payload
        std::string text;
        int key = 0;
        // 修饰键（GLFW 掩码：1=Shift 2=Ctrl 4=Alt 8=Super）。键盘/指针事件都如实带上；
        // 焦点类与文本类事件恒为 0。
        int mods = 0;
        // 产生本次事件的指针按键（mouse_button::kXxx）。MouseDown / MouseUp 如实带上按键 ——
        // 要区分左右键就在这两类处理器里看它；Click 只会是左键（右键不产生 Click）。
        // 键盘 / 文本 / 焦点类事件与指针无关，恒为默认值。
        int button = mouse_button::kLeft;
    };

    // Selector (supports #id and .class)
    struct Selector {
        enum class Type { Id, Class };
        Type type;
        std::string value;
    };

    // parse simple selector string (#id or .class)
    static Selector parseSelector(const std::string& s);

    // 处理器签名用 `UIEvent&`，可在处理器内置 ev.stopPropagation（DOM 语义）；
    // `const UIEvent&` 签名也可绑定。
    using EventCallback = std::function<void(UIEvent&)>;

    // bind a handler
    void bind(const std::string& selector, UIEventType type, EventCallback cb);

    // dispatch an event；`e` 非 const：处理器可以直接改 stopPropagation
    void dispatch(UIEvent& e);
    void flushPendingHandlers();
private:

    struct Handler {
        Selector selector;
        UIEventType type;
        EventCallback cb;
    };

    std::vector<Handler> handlers_;
    std::vector<Handler> pendingHandlers_;
    int dispatchDepth_ = 0;

    FocusObserver focusObserver_;


    // store ancestor paths (root -> node) for hover/active so we can add/remove state along the path
    std::vector<ResolvedNode*> hoverPath_;
    std::vector<ResolvedNode*> activePath_;
    ResolvedNode* focusNode_ = nullptr;
    std::unordered_set<AbstractControl*> pressedControls_;
};

} // namespace loppe