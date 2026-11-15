/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// undo_redo.h — 通用撤销/重做管理器（快照式 + 连续操作合并）
#pragma once

#include <vector>
#include <chrono>
#include <cstddef>

namespace loppe {

// 通用撤销/重做管理器：
// - 只管理"快照栈 + 连续操作合并"，不关心状态类型（T 需可拷贝/移动）；
// - 使用方在每次修改前 push(修改前状态)；撤销/重做时传入当前状态、取回目标状态自行应用。
template <typename T>
class UndoRedo {
public:
    // 记录一次"修改前"状态。typing/deleting 用于连续同类型操作合并：
    // 时间窗（500ms）内同类型操作并入上一步，不新增快照。
    void push(const T& preState, bool typing = false, bool deleting = false);

    // 撤销：把 current 压入 redo 栈，弹出上一个快照到 out。栈空返回 false。
    bool undo(const T& current, T& out);

    // 重做：把 current 压入 undo 栈，弹出下一个快照到 out。栈空返回 false。
    bool redo(const T& current, T& out);

    // 清空全部撤销/重做快照（如程序化设值后调用）。
    void clear();

    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }

private:
    static constexpr std::size_t kMaxStack = 200;
    static constexpr int kCoalesceWindowMs = 500;

    std::vector<T> undoStack_;
    std::vector<T> redoStack_;
    bool coalesceTyping_ = false;
    bool coalesceDeleting_ = false;
    std::chrono::steady_clock::time_point lastEditTime_ = std::chrono::steady_clock::now();
};

// ---- 实现（模板，需在头文件）----

template <typename T>
void UndoRedo<T>::push(const T& preState, bool typing, bool deleting) {
    auto now = std::chrono::steady_clock::now();
    auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEditTime_).count();
    bool coalesce = false;
    if (typing && coalesceTyping_ && dt < kCoalesceWindowMs) coalesce = true;
    if (deleting && coalesceDeleting_ && dt < kCoalesceWindowMs) coalesce = true;
    coalesceTyping_ = typing;
    coalesceDeleting_ = deleting;
    lastEditTime_ = now;
    if (coalesce && !undoStack_.empty()) return;   // 并入上一步
    undoStack_.push_back(preState);
    if (undoStack_.size() > kMaxStack) undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

template <typename T>
bool UndoRedo<T>::undo(const T& current, T& out) {
    if (undoStack_.empty()) return false;
    T prev = undoStack_.back();
    undoStack_.pop_back();
    redoStack_.push_back(current);
    coalesceTyping_ = coalesceDeleting_ = false;
    out = std::move(prev);
    return true;
}

template <typename T>
bool UndoRedo<T>::redo(const T& current, T& out) {
    if (redoStack_.empty()) return false;
    T next = redoStack_.back();
    redoStack_.pop_back();
    undoStack_.push_back(current);
    if (undoStack_.size() > kMaxStack) undoStack_.erase(undoStack_.begin());
    coalesceTyping_ = coalesceDeleting_ = false;
    out = std::move(next);
    return true;
}

template <typename T>
void UndoRedo<T>::clear() {
    undoStack_.clear();
    redoStack_.clear();
    coalesceTyping_ = coalesceDeleting_ = false;
}

} // namespace loppe