/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// key_repeat.h — 长按重复状态机（退格 / 删除等按住连发）
#pragma once

namespace loppe {

class KeyRepeat {
public:
    // 按下；initialDelayMs / intervalMs 单位均为毫秒
    void press(int initialDelayMs = 400, int intervalMs = 50);
    void release();
    bool isHeld() const;
    // 每帧调用，frameDtMs 为帧间隔（毫秒）；返回 true 表示本轮应触发一次重复动作
    bool update(int frameDtMs);

private:
    bool held_ = false;
    int timerMs_ = 0;
    int initialDelayMs_ = 400;
    int intervalMs_ = 50;
};

} // namespace loppe
