/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// text_edit_ops.h — 文本编辑原语
//
// 所有函数以 (TextBuffer, TextSelection) 为输入输出；
// 返回"新光标位置"（-1 表示未发生删除/插入），由调用方决定如何更新选区。
#pragma once

#include "text_buffer.h"
#include "text_selection.h"
#include <string>

namespace loppe {
namespace textedit {

// UTF-8 字符串的码点数
int utf8CodepointCount(const std::string& utf8);

// 删除选区（若有）；返回删除后的光标位置（= 选区起点）；无选区返回 -1
int deleteSelection(TextBuffer& buf, const TextSelection& sel);

// 在光标处插入 utf8；若存在选区先替换选区。返回插入后的光标位置
int insertText(TextBuffer& buf, const TextSelection& sel, const std::string& utf8);

// 覆盖模式：替换光标处单个码点（光标在末尾则退化为插入）。返回新光标位置
int overwriteText(TextBuffer& buf, const TextSelection& sel, const std::string& utf8);

// 删除光标前一个码点（有选区则删选区）。返回新光标位置；无删除返回 -1
int deletePrevious(TextBuffer& buf, const TextSelection& sel);

// 删除光标处码点（有选区则删选区）。返回新光标位置；无删除返回 -1
int deleteNext(TextBuffer& buf, const TextSelection& sel);

// 全选
void selectAll(const TextBuffer& buf, TextSelection& sel);

// 取选区文本（无选区返回空串）
std::string selectedText(const TextBuffer& buf, const TextSelection& sel);

} // namespace textedit
} // namespace loppe
