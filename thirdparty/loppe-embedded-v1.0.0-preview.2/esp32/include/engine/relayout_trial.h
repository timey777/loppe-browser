/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "components/uitypes.h"
#include "loppe_export.h"

#include <cstddef>
#include <vector>

class DocumentRuntime;

namespace loppe {

class ResolvedNode;

// 一次「局部重排尝试」：把 Layout 脏的节点各自重排一遍，再看盒子有没有变。
// 成功 = 兄弟布局不受影响，调用方按 touchedNodes/旧外框同步与重绘；失败或未 commit 析构时自动还原 Layout 脏标记。
//
// 用法（唯一调用点是 ViewCoordinator::runPass 的「重排类先试局部」那一段）：
//     RelayoutTrial trial(runtime);
//     if (trial.begin(plan.dirtyNodes)) {          // false = 预检不过，树没被动过
//         trial.layout();
//         if (trial.stable()) {                    // false = 析构自动回滚脏标记
//             sync(trial.touchedNodes());
//             trial.commit();
//         }
//     }
//
// 生命周期：begin 之后、析构之前用完；不可拷贝。
class LOPPE_API RelayoutTrial {
public:
    explicit RelayoutTrial(::DocumentRuntime* runtime);
    ~RelayoutTrial();

    RelayoutTrial(const RelayoutTrial&) = delete;
    RelayoutTrial& operator=(const RelayoutTrial&) = delete;

    // 预检 + 记账（不动树）。false = 局部不成立：没有 Layout 脏节点 / 根节点自己脏 /
    // 子项尺寸归父级 resolve。返回 false 后可直接丢弃本对象。
    bool begin(const std::vector<ResolvedNode*>& dirtyNodes);

    // 试排：逐节点重算布局（位置基准为记账时的 margin box）。
    void layout();

    // 尺寸与位置都没变（阈值 0.5px）。只能在 layout() 之后调用。
    bool stable() const;

    // 试排覆盖的整棵子树（去重、跳过待移除节点）。只能用 stable() 为真时。
    std::vector<ResolvedNode*> touchedNodes() const;

    // 记账时的旧边框盒（布局后旧位置从这里取）。
    size_t size() const;
    ResolvedNode* nodeAt(size_t i) const;
    const ui::Rect& oldBorderBoxAt(size_t i) const;

    // 落地：清 Paint 脏 + 清祖先 ChildrenLayout。
    void commit();

private:
    struct Entry {
        ResolvedNode* node = nullptr;
        ui::Rect borderBox;
        ui::Rect marginBox;
    };

    void rollback();

    ::DocumentRuntime* runtime_ = nullptr;
    std::vector<Entry> entries_;
    bool laidOut_ = false;
    bool committed_ = false;
};

} // namespace loppe
