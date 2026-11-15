/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <chrono>

namespace loppe {

// =========================================================================
// Throttle — 通用"间隔内最多执行一次"限流门
//
// 用法：
//   loppe::Throttle t;                          // 默认 16ms（约 60fps）
//   if (t.tryAcquire()) { doExpensiveWork(); }
//
// tryAcquire() 只有在上一次成功获取（或构造）之后经过了最小间隔时才返回
// true，并记录当前时刻作为新的参考点。间隔设为 0 表示不限流（每次都通过）。
//
// 仅限单线程使用，内部不做同步。
// =========================================================================
class Throttle {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;

    // 默认间隔 16ms（约 60fps）；0 表示不限流。
    explicit Throttle(Duration minInterval = std::chrono::milliseconds(16));

    // 距上次成功获取已超过最小间隔则返回 true，并刷新参考时刻。
    bool tryAcquire();

    // 把参考时刻重置为当前时刻（下一次 tryAcquire 必须等待完整间隔）。
    void reset();

    void setMinInterval(Duration interval);
    Duration minInterval() const;

private:
    Duration minInterval_;
    Clock::time_point lastTime_{};
};

} // namespace loppe