/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct ASTNode;

namespace loppe {

    // 节点 uid 分两个区间：源码空间（uid = kSourceBase + 源码起始字节偏移）与合成空间（uid >= kSyntheticBase，进程内单调分配）。
    using NodeUid = uint32_t;

    constexpr NodeUid kUidNone = 0;                  // 未分配（无身份）
    constexpr NodeUid kUidRoot = 1;                  // 文档根：固定身份，不随编辑变
    constexpr NodeUid kSourceBase = 2;               // 源码空间起点（offset 0 → kSourceBase）
    constexpr NodeUid kSyntheticBase = 0x80000000u;  // 合成空间起点

    bool isSourceUid(NodeUid uid);
    bool isSyntheticUid(NodeUid uid);

    struct StableId {
        // 源码字节偏移 → 源码空间 uid（超出可表示范围返回 kUidNone 并报错）
        static NodeUid fromOffset(size_t byteOffset);

        // 递归给「来自源码」的节点补 uid（只填 uid == kUidNone 的格子，幂等）。
        // root 必须是**文档根**（它拿 kUidRoot）。新解析出来的 AST 必须在 diff **之前**调用。
        static void assignSourceUids(const std::shared_ptr<ASTNode>& root);

        // 进程内单调分配的合成 uid（不复用已分配的值）。
        static NodeUid nextSynthetic();

        // 整数 ↔ 字符串（app 边界与日志用）。
        static std::string format(NodeUid uid);
        static NodeUid parse(const std::string& text);

        // 给「不在当前源码里」的子树补合成身份（克隆/挂载进来的子树）。
        // 只给还没有合成身份的节点分配：已有合成身份的节点保持不动。
        static void ensureSyntheticUids(const std::shared_ptr<ASTNode>& root);
    };

    // 一次编辑的坐标变换：把旧文件的 uid 映射到新坐标系。
    class SourceRelocation {
    public:
        // 一处改动：旧文件的 [oldStart, oldEnd) 换成了新文件的 [newStart, newEnd)
        struct Span {
            size_t oldStart = 0;
            size_t oldEnd = 0;
            size_t newStart = 0;
            size_t newEnd = 0;
        };

        static SourceRelocation between(const std::string& oldText, const std::string& newText);

        // 旧 uid → 新坐标系下的 uid。合成空间原样返回。
        // 落在某处改动**被替换/删除的旧字节**里的节点返回 kUidNone
        //（那些节点在新文件里已经没有对应位置了 —— 语义上就是「删掉」，不是兜底）；
        // 例外：等长替换（净位移 0）不改坐标，区间内部的位置照旧有效。
        NodeUid mapOld(NodeUid oldUid) const;

        bool moved() const;
        const std::vector<Span>& spans() const;
        // 整段改写量（诊断用）：旧坐标下被改动覆盖的字节数
        size_t removedBytes() const;

    private:
        std::vector<Span> spans_;
    };

} // namespace loppe