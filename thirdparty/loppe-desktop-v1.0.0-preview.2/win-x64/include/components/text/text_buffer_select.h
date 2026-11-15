/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// text_buffer_select.h — TextBuffer 版选区助手
//
//   · wordRange    —— 词段（[start, end)，码点索引）
//   · lineRange    —— 整逻辑行（不含行尾 '\n'；单行文本 = 全文）
//   · dragActiveCp —— 拖拽时把位置吸附成选区活动端（方向由 pos 与锚点比较决定）
#pragma once

#include "text_segment.h"
#include "multi_click_select.h"
#include "components/input/text_buffer.h"

namespace loppe {
namespace textsel {

// 词段 [start, end)（码点索引；越界钳位、空文本返回 {0,0}）
textseg::Range wordRange(const TextBuffer& buf, int cp);

// 整逻辑行 [start, end)（码点索引，不含行尾 '\n'；空文本返回 {0,0}）
textseg::Range lineRange(const TextBuffer& buf, int cp);

// 拖拽吸附：unit=Word/Line 时活动端吸到词/行边界；方向由 pos 与锚点比较决定（同侧向后取尾、向前取首）
int dragActiveCp(const TextBuffer& buf, int anchorCp, DragUnit unit, int pos);

} // namespace textsel
} // namespace loppe

