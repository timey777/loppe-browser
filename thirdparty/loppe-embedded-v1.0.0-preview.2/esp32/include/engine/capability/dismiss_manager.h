/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <functional>
#include <vector>

#include "engine/core/resolved_node.h"

namespace loppe {

// 弹层（菜单/下拉/浮窗）的「外部点击 / 窗口失焦 → 自动关闭」管理器。
//
// 用法（弹层组件）：
//   打开：scene->getDismissManager().add(panelNode, cb)；关闭：remove(panelNode)。
// cb(hit, button) 里的 hit 供注册方做豁免（如「菜单头」）；button = 触发按下的按钮
// （0 左 / 1 右；失焦路径为 -1）。返回 false = 只关闭 / 不拦截（不吞本次按下）。
class LOPPE_API DismissManager {
public:
    using Callback = std::function<bool(ResolvedNode* hit, int button)>;

    // 打开弹层时注册（重复注册同 panel = 替换回调）；关闭时 remove。
    void add(ResolvedNode* panel, Callback cb);
    void remove(ResolvedNode* panel);

    // 鼠标按下：命中面板外 → 依次回调（button 原样透传）；返回 true = 发生了 dismiss（吞掉本次按下）。
    bool onMouseDown(ResolvedNode* hit, int button);

    // 窗口失焦：全部回调（hit = nullptr，一律视为外部）。
    void dismissAll();

    // 节点被移除（panel 自身或其子树内）→ 注册作废，不回调。
    void onNodeRemoved(ResolvedNode* node);

    // 场景 detach：清空全部注册（不回调）。
    void clear();

private:
    struct Entry {
        ResolvedNode* panel = nullptr;
        Callback cb;
    };

    std::vector<Entry> entries_;
};

} // namespace loppe
