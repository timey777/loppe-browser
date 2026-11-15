/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// input_source.h —— 指针事件的来源（触摸 / 鼠标）。控件与宿主按它区分交互方式。

#include <cstdint>

namespace loppe {

enum class InputSource : uint8_t {
    Mouse = 0,
    Touch,
    // 「没指定」：事件里带这个值 = 沿用当前来源（不改）。
    Unspecified
};

} // namespace loppe
