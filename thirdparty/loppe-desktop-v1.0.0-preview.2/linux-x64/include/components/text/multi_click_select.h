/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// multi_click_select.h — 多击（单击/双击/三击）判定状态机
//
// 只做判定（几连击 + 拖拽粒度），不碰文本模型与选区：把结果落成选区由宿主控件做。
#pragma once

namespace loppe {
namespace textsel {

// 拖拽粒度：单击 = 字符；双击后 = 词；三击后 = 行（吸附规则由宿主按文本模型实现）
enum class DragUnit { Char, Word, Line };

class MultiClickSelect {
public:
    struct Policy {
        double multiClickMs = 400.0;   // 连击时间窗（毫秒，超过则重新从 1 计）
        float  slopDp      = 4.0f;     // 连击位置容差（本地坐标，dp）
    };

    void setPolicy(const Policy& p);
    const Policy& policy() const;

    // 鼠标按下：返回本次击数（1 / 2 / 3+）并更新拖拽粒度（1→Char、2→Word、>=3→Line）。
    // 时间由调用方给（毫秒单调时钟），便于测试注入；onPress(x, y) 用 steady_clock。
    int onPress(float x, float y, double nowMs);
    int onPress(float x, float y);

    int clickCount() const;
    DragUnit unit() const;

private:
    int clickCount_ = 0;
    DragUnit unit_ = DragUnit::Char;
    bool hasLast_ = false;
    double lastMs_ = 0.0;
    float lastX_ = 0.0f;
    float lastY_ = 0.0f;
    Policy policy_;
};

} // namespace textsel
} // namespace loppe
