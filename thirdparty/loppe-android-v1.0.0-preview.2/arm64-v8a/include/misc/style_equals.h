/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "engine/core/computed_style.h"
#include "engine/core/node_flags.h"
#include "loppe_export.h"

namespace loppe {

// 逐字段比较两个计算样式是否**完全相同**。
LOPPE_API bool styleEquals(const ComputedStyle& a, const ComputedStyle& b);

// 只比较影响布局的字段（盒模型、文本测量、overlay 定位）；颜色 / 圆角 / 透明度等纯表现字段不参与。
LOPPE_API bool styleLayoutEquals(const ComputedStyle& a, const ComputedStyle& b);

// 只比较「盒尺寸」字段：width / height / min-max / margin（含 margin-auto）。
LOPPE_API bool styleSizeEquals(const ComputedStyle& a, const ComputedStyle& b);

// 比较除盒尺寸以外的全部字段（= 布局非尺寸部分 + 纯表现部分）。
LOPPE_API bool styleNonSizeEquals(const ComputedStyle& a, const ComputedStyle& b);

// 由样式前后差异派生应标记的脏位：仅盒尺寸变化 → Layout|Paint；其它变化额外带 Style；返回 DirtyFlag::None 表示样式没变。
LOPPE_API DirtyFlag deriveDirtyFlags(const ComputedStyle& prev, const ComputedStyle& next);

} // namespace loppe

