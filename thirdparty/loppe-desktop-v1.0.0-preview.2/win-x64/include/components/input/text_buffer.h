/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// text_buffer.h — UTF-8 文本内容模型（codepoint 编辑单位 + byte 映射 + 行索引）
//
// 文本数据核心：
// - 所有编辑接口以 codepoint 为索引单位，内部维护 byte<->codepoint 映射；
// - 维护按 '\n' 划分的行索引（byte 区间），供逐行渲染 / 行号 / 装饰使用。
#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace loppe {

class TextBuffer {
public:
    // 一行在原始字节流中的区间（length 不含行尾 '\n'）
    struct LineRange {
        size_t startByte = 0;
        size_t length = 0;
    };

    TextBuffer() = default;
    explicit TextBuffer(const std::string& text);

    // ---- 读取 ----
    const std::string& text() const;   // 整篇文本（编辑后首次调用合成并缓存）
    bool empty() const;
    size_t size() const;

    // codepoint 数量
    int codepointCount() const;

    // codepoint 索引 -> byte 偏移（cp == count 时返回 text 末尾）
    int codepointIndexToByte(int cpIndex) const;
    // byte 偏移 -> codepoint 索引（非码点边界取右侧最近边界）
    int byteIndexToCodepoint(int byteIndex) const;
    // 取子串 [startCp, endCp)
    std::string substring(int startCp, int endCp) const;

    // ---- 行索引 ----
    int lineCount() const;
    const LineRange& line(int lineIndex) const;
    // codepoint 位置所在行
    int lineAt(int cpIndex) const;
    // 行首 / 行尾（不含 '\n'）的 codepoint 位置
    int lineStartCp(int lineIndex) const;
    int lineEndCp(int lineIndex) const;
    // 该行文本（不含 '\n'）
    std::string lineText(int lineIndex) const;

    // ---- 快速按行访问 ----
    // 按行访问，不合成整篇文本。
    bool lineByteRange(int lineIndex, int* outStartByte, int* outEndByteExclusive) const;
    std::string lineTextFast(int lineIndex) const;
    int byteToCpFast(int byteIndex) const;

    // ---- 编辑（参数均为 codepoint 索引；编辑后缓存自动失效）----
    void setText(const std::string& text);
    void insert(int cp, const std::string& utf8);
    void erase(int startCp, int endCp);            // [startCp, endCp)
    void replace(int startCp, int endCp, const std::string& utf8);
    void eraseCodepointAt(int cp);


private:
    struct Piece {
        int src = 0;
        int start = 0;
        int len = 0;
        int cps = 0;
        int nl = 0;
    };
    std::string base_;
    std::string add_;
    std::vector<Piece> pieces_;
    int totalBytes_ = 0;
    int totalCps_ = 0;
    int totalNewlines_ = 0;

    mutable bool composedDirty_ = true;
    mutable std::string text_;
    mutable std::vector<int> cpToByte_;
    mutable std::vector<LineRange> lines_;
    mutable bool mapped_ = false;

    void invalidateCache() const;
    void ensureComposed() const;
    void ensureMapped() const;

    void markComposedDirty();
    void rebuildTotals();
    void coalesce();
    void maybeCompact();
    int  cpToByteNoCompose(int cp) const;
    void removeBytes(int a, int b);
    int  byteAfterNthNewline(int nth) const;
    std::string readBytes(int a, int b) const;
    static void appendLineChunks(const char* data, int len, int src, int startOff,
                                 std::vector<Piece>& out);
};

} // namespace loppe