/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <string>
#include <cstdint>
#include <limits>
#include "misc/float_utils.h"   // sameConstraint / packDefinite（NaN 感知比较、definite 位打包）

namespace loppe {

// 测量约束 key 值类型：父级可用内容尺寸 (w,h) + 各轴 effective definite；equals 对 auto/无界约束做 NaN 感知比较。
class MeasureConstraint {
public:
    MeasureConstraint() = default;
    MeasureConstraint(float parentW, float parentH, uint8_t effectiveDefinite);

    // 由父级约束构造 key：按各轴 auto 与否屏蔽 definite 位（bit0=widthDefinite, bit1=heightDefinite）。
    static MeasureConstraint forPhase(float parentW, float parentH, uint8_t rawDefinite,
                                      bool widthAuto, bool heightAuto,
                                      bool heightDependsOnParent = true);

    float width() const;
    float height() const;
    uint8_t effectiveDefinite() const;

    // NaN 感知的相等比较（sameConstraint）：UNBOUNDED / auto 与自身相等。
    bool equals(const MeasureConstraint& o) const;

private:
    // 与测量结果无关的轴的占位值（NaN）。
    static constexpr float kUnrelated = std::numeric_limits<float>::quiet_NaN();

    float parentW_ = 0;
    float parentH_ = 0;
    uint8_t effectiveDefinite_ = 0;  // 已按 auto 轴屏蔽后的 definite 位
};

// 测量去重缓存：记录节点最近一次「测量 / 重测 / 文本测量」的输入 key；命中且子树干净时复用上次结果。
// 线程模型：单 UI 线程使用，无锁。
class MeasureCache {
public:
    // ---- measurePhase 约束缓存 ----
    // key = MeasureConstraint（父级可用尺寸 + 各轴 effective definite，auto 轴 definite 不参与）。
    // 命中条件：与上次真实测量的 key 完全相同（NaN 视为相等）且调用方另行校验子树干净。
    bool matchesConstraint(const MeasureConstraint& key) const;
    void storeConstraint(const MeasureConstraint& key, uint64_t frame);

    // ---- reMeasureChildren 入口 memo ----
    // key = 入口时自身 content box (w, h) + definite 位掩码（resolve 侧显式要求的 definite，不做 auto 屏蔽）
    // 命中条件：与上次重测的输入完全相同且调用方另行校验子树干净。
    bool matchesRemeasure(float w, float h, uint8_t definite) const;
    void storeRemeasure(float w, float h, uint8_t definite, uint64_t frame);

    // 本帧是否被实际重新测量过（currentFrame 为当前帧号）。
    bool measuredThisFrame(uint64_t currentFrame) const;

    // ---- 文本测量缓存 ----
    // 文本 + 字体（family+size+bold/italic）相同，且 wrapW 与上次精确相同（或上次结果不换行且新 wrapW 足够）时命中。
    struct TextEntry {
        std::string text;
        std::string family;
        float size = 0;
        bool bold = false;
        bool italic = false;
        float wrapW = 0;
        float width = 0;
        float ascent = 0, descent = 0, lineHeight = 0;
        bool noWrap = false;
        bool valid = false;
    };
    TextEntry& text();
    const TextEntry& text() const;

private:
    // 存槽：key + 是否有效（measure 与 remeasure 共用）。
    struct StoredKey {
        MeasureConstraint key;
        bool valid = false;
    };

    StoredKey constraint_;   // measurePhase 最近一次真实测量的 key
    StoredKey remeasure_;    // reMeasureChildren 最近一次输入
    TextEntry text_;
    uint64_t lastMeasuredFrame_ = 0;  // 最近一次实际测量的帧号
};

} // namespace loppe