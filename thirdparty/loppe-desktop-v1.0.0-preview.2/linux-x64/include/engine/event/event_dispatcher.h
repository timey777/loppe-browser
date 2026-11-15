/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <unordered_set>
#include <memory>
#include <vector>
#include "engine/core/resolved_tree.h"
#include "engine/core/coordinate_types.h"
#include "components/input/input_event.h"
#include "components/hit_test_helper.h"
#include "components/abstract_control.h"
namespace loppe {
class UIEventSystem;
class InteractionScope;
class InteractionScopeRegistry;

/**
 * @brief 命中路径按「交互作用域」切段的结果
 *
 * path 是 root→leaf。槽锚节点（在宿主树里）下标 = `boundary`；它**之后**的节点
 * （`innerFrom..end`）属于槽的作用域。
 */
struct InteractionSegments {
    InteractionScope* inner = nullptr;   // 最深那层的作用域；nullptr = 整条路径都是宿主的
    size_t innerFrom = 0;                // 内段起点（path 下标，含）；无作用域时忽略
    size_t boundary = 0;                 // 槽锚节点下标（内段的父边界）
    bool bubblesToHost = true;           // 作用域链上没有 Isolated 时才继续冒到宿主
    std::vector<InteractionScope*> chain;  // 由内到外的作用域链（iframe 嵌套逐层分发用）
    std::vector<size_t> anchors;           // 与 chain 一一对应：各槽锚在 path 里的下标
};

/**
 * @brief 事件分发器 - 负责将鼠标/键盘事件分发到控件树
 */
class EventDispatcher {
public:
    /**
     * @brief 分发鼠标按钮事件到路径上的控件
     * @param path 控件路径（从根到叶）
     * @param e 按键事件（e.point 为逻辑坐标 DIP）
     * @param uiEventSystem 宿主事件域
     * @param scopes 交互作用域注册表（可空）；各层预览用各自 scope->events()
     * @return 是否被处理
     */
    static bool dispatchMouseButton(
        const std::vector<ResolvedNode*>& path,
        const MouseButtonEvent& e,
        UIEventSystem* uiEventSystem,
        const InteractionScopeRegistry* scopes = nullptr);

    /**
     * @brief 分发鼠标移动事件到路径上的控件
     * @param path 控件路径（从根到叶）
     * @param e 移动事件（e.point 为逻辑坐标 DIP）
     * @param scopes 交互作用域注册表（可空）
     */
    static void dispatchMouseMove(
        const std::vector<ResolvedNode*>& path,
        const MouseMoveEvent& e,
        const InteractionScopeRegistry* scopes = nullptr);

    /**
     * @brief 分发滚轮事件
     * @param ctrl 起始控件
     * @param point 逻辑坐标（DIP）
     * @param e 滚轮事件（e.delta 为滚动增量，逻辑 DIP）
     * @return 是否被处理
     */
    static bool dispatchScroll(
        std::shared_ptr<AbstractControl> ctrl,
        LogicalPointDp point,
        const ScrollEvent& e);

    /**
     * @brief 把命中路径按交互作用域切段（内层段 = 槽子树那段）
     * @param path 命中的 ResolvedNode 路径（root→leaf）
     * @param registry 视图的交互作用域注册表（Scene 的 ResolvedTreeView）
     */
    static InteractionSegments segmentPath(const std::vector<ResolvedNode*>& path,
                                           const InteractionScopeRegistry& registry);

private:
    /**
     * @brief 处理overlay（如滚动条）的鼠标按钮事件
     */
    static bool dispatchOverlayMouseButton(
        const std::vector<ResolvedNode*>& path,
        const std::vector<NodeLocalPointDp>& pathCoords,
        const MouseButtonEvent& e);

    /**
     * @brief 处理overlay的鼠标移动事件
     */
    static void dispatchOverlayMouseMove(
        const std::vector<ResolvedNode*>& path,
        const std::vector<NodeLocalPointDp>& pathCoords);
};

} // namespace loppe