/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// caret_blink.h — 光标闪烁状态机
#pragma once

#include <chrono>

namespace loppe {

class CaretBlink {
public:
    // 开始闪烁：立即显示并重新计时
    void enable();
    // 停止闪烁并隐藏
    void disable();
    bool enabled() const;
    bool visible() const;
    // 立即显示并重新计时（点击 / 按键后调用）
    void reset();
    // 每帧驱动；返回 true 表示可见性发生变化（调用方应触发重绘）
    bool update();

private:
    static constexpr int kBlinkIntervalMs = 500;
    bool enabled_ = false;
    bool visible_ = false;
    std::chrono::steady_clock::time_point lastToggle_;
};

} // namespace loppe
