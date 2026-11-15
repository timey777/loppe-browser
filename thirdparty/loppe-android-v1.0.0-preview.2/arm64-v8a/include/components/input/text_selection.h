/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// text_selection.h — 光标 + 选区模型（codepoint 索引）
//
// anchor != active 表示存在选区；active 即光标位置（文档流中的插入点）。
#pragma once

namespace loppe {

struct TextSelection {
    int anchor = 0;   // 选区锚点
    int active = 0;   // 选区活动端（== 光标）

    int caret() const;
    bool hasSelection() const;
    int start() const;
    int end() const;

    void collapse(int pos);
    void set(int a, int act);
    // 将位置夹到 [0, maxCp]
    void clamp(int maxCp);
};

} // namespace loppe
