/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// UTF-8 / UTF-32 utility helpers
#pragma once

#include <string>
#include <cstdint>

#include <vector>

// Convert UTF-8 encoded string to UTF-32 codepoint string
std::u32string utf8_to_utf32(const std::string& s);

// Convert UTF-32 string to UTF-8 encoded string
std::string utf32_to_utf8(const std::u32string& s);

// Convert UTF-8 to platform wide string (`std::wstring`). On Windows this
// produces UTF-16 in `wchar_t` (surrogate pairs for codepoints > 0xFFFF).
std::wstring utf8_to_wstring(const std::string& s);

// Convert platform wide string to UTF-8. Handles UTF-16 surrogate pairs on Windows.
std::string wstring_to_utf8(const std::wstring& ws);

// Decode UTF-8 into codepoints and also return the starting byte index for each codepoint.
// outByteIndex will have same length as returned u32string; outByteIndex[i] is the
// byte offset in the original string where codepoint i starts.
std::u32string utf8_to_utf32_with_offsets(const std::string& s, std::vector<int>& outByteIndex);

// 返回 p[0] 起始的码点占用的字节长度（1..4）；p 处剩余字节数 remainingBytes == 0 时返回 0。
// 判定规则与 utf8_to_utf32 / utf8_to_utf32_with_offsets 完全一致：
// 非法首字节 / 截断 / 过长编码 / 代理区 均视为消耗 1 字节（解码端按替换码点 U+FFFD 处理）。
// 供需要"逐码点推进但不想分配 u32 串"的调用方使用（如增量文本模型 TextBuffer）。
int utf8_seq_len_at(const char* p, size_t remainingBytes);

// 单遍统计一段 UTF-8：码点数与 '\n' 行数（'\n' 为单字节 ASCII，不参与多字节序列）。
// 码点边界判定与 utf8_to_utf32(_with_offsets) / utf8_seq_len_at 一致。n==0 时均为 0。
struct Utf8Counts {
    int codepoints = 0;
    int newlines = 0;
};
Utf8Counts utf8_count(const char* p, size_t n);

// 码点计数，等价于 utf8_to_utf32(s).size()。
int utf8_count_codepoints(const char* p, size_t n);
int utf8_count_codepoints(const std::string& s);