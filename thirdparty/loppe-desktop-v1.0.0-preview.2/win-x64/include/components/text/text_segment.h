/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// text_segment.h — 「双击选词」用的轻量文本分段（无词典启发式）
//
// 给定 UTF-8 文本 + 位置，返回该位置所属的「段」（= IDE 双击选中的范围）。
// 段类别：Word（单词/普通文字连续段）、CjkUnit（CJK 单字成段）、
//         Space（连续空白，含全角空格）、Punct（同一码点的连续跑）。
// 前置条件：输入为合法 UTF-8（非法/截断序列按 U+FFFD 处理）；
//           段边界一定落在码点边界上（不会切开 UTF-8 序列）。
//
// 示例：
//   loppe::textseg::Range r = wordRangeAt("hello 世界", 7);  // "世"
#pragma once

#include <cstddef>
#include <string>

namespace loppe {
namespace textseg {

// 字符类别
enum class CharClass { Space, Word, CjkUnit, Punct };

// 一段文本（码点或字节索引，语义由调用接口决定；[start, end) 半开区间）
struct Range {
    int start = 0;
    int end = 0;
    int len() const;
    bool empty() const;
};

namespace detail {

// 单个码点：起始字节 + 字节长度 + 码点值（非法/截断序列按 U+FFFD）
struct Cp {
    size_t byte = 0;
    int len = 0;
    unsigned int value = 0;
};

// 解码 s[byte] 处的一个码点；返回 false = 已到末尾
bool decodeAt(const std::string& s, size_t byte, Cp* out);

} // namespace detail

// ---- 码点分类 ----

bool isAsciiSpace(unsigned int c);
bool isAsciiWord(unsigned int c);
// 空白（Unicode）：含 NBSP / 各种宽度空格 / 行分隔 / 全角空格
bool isSpaceCp(unsigned int c);
// CJK 表意/音节字符（Han / 假名 / 注音）—— 单字成段
bool isCjkUnitCp(unsigned int c);
// 码点分类
CharClass classifyCp(unsigned int c);

namespace detail {

// 相邻码点是否属于同一段
bool sameRun(CharClass cls, unsigned int val, CharClass prevCls, unsigned int prevVal);

} // namespace detail

// 第 cp 个码点处的段（cp 越界钳位到 [0, 码点数-1]；空文本返回 {0,0}）
Range wordRangeAt(const std::string& utf8, int cp);

// 单行文本中字节位置 byte 处的段（byte 自动对齐到所在码点的起始；空文本返回 {0,0}）
Range wordRangeAtByte(const std::string& utf8, int byte);

} // namespace textseg
} // namespace loppe
