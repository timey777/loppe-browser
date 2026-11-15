/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

#include "loppe_export.h"

namespace loppe {

/**
 * @brief UI 线程任务派发器：任意线程 post，UI 线程（主循环）drain。
 *
 * 用途：后台线程把任务投递到 UI 线程执行（如改控件状态、切界面）。
 *
 * 线程：`post` 任意线程可调；`drain` / `hasPending` / `discardPending` 只在 UI 线程调。
 * 控件/节点不是线程安全的，后台线程不要直接碰。
 */
class LOPPE_API UiDispatcher {
public:
    static UiDispatcher& instance();

    // 任意线程可调：入队一个要在 UI 线程执行的任务（空函数被忽略）。
    void post(std::function<void()> task);

    // UI 线程（主循环每 tick）：执行当前挂起的全部任务（FIFO）。
    void drain();

    bool hasPending() const;

    // 丢弃仍挂起的任务并返回条数（用于关停阶段）。
    std::size_t discardPending();

private:
    UiDispatcher() = default;
    UiDispatcher(const UiDispatcher&) = delete;
    UiDispatcher& operator=(const UiDispatcher&) = delete;

    mutable std::mutex mu_;
    std::vector<std::function<void()>> queue_;
};

} // namespace loppe
