/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "engine.h"
#include <map>
#include <unordered_map>
#include <string>
#include "engine/core/node_flags.h"
#include "text_measure_cache.h"
namespace loppe { class ResolvedNode; }
// 盒模型计算引擎
class BoxModelEngine : public AbstractEngine {
public:
    BoxModelEngine();
    ~BoxModelEngine();
    
    void initialize() override;
    void shutdown() override;
    
    std::type_index type() const override;
    void process(std::shared_ptr<ASTNode> root, loppe::DisplayList& outList, 
                 float viewportW, float viewportH) override;
    
    std::string name() const override;
    
    // 热重载支持：增量更新
    void onASTUpdated(std::shared_ptr<ASTNode> root) override;
    
    
    // 重置引擎状态
    void reset();

    // Measure & layout pass methods (converted from free functions)
    void measurePhase(loppe::ResolvedNode* node, float containerContentW, float containerContentH, loppe::FormattingContext parentCtx, bool parentWidthDefinite = true, bool parentHeightDefinite = true);
    // 定位之前必须先跑 resolvePhase。
    void resolvePhase(loppe::ResolvedNode* node);
    // Re-measure children of a node that has been resized (e.g. by flex-grow/flex-shrink
    // or align-items:stretch).
    // widthDefinite/heightDefinite: true when the caller has authoritatively resolved that axis.
    void reMeasureChildren(loppe::ResolvedNode* node, bool widthDefinite, bool heightDefinite);

    // Re-calculates positions for node and its children based on current sizes and parent coordinates.
    // Skips measurement phase. Use this for things like dragging or simple position updates.
    void layoutSubtree(loppe::ResolvedNode* node);

    void layoutPhase(loppe::ResolvedNode* node, float parentContentX, float parentContentY);
private:
	// Handle absolute positioning logic for overlays (center/anchor + offsetX/Y)
	void applyAnchor(loppe::ResolvedNode& node, const loppe::ResolvedNode& parent);

    // Read margin/padding/border from StyleEngine / attributes
    void readBoxModelProperties(loppe::ResolvedNode* node);

    // Content measurement helpers
    void measureContent(loppe::ResolvedNode* node, float& requestedW, float& requestedH, float availableWidth);

    bool  hasLaidOut_    = false;
    float lastViewportW_ = 0.0f;
    float lastViewportH_ = 0.0f;

public:
    // Scroll state calculation (public for external recalculation, e.g. preview attach)
    void calculateScrollState(loppe::ResolvedNode* node);
};

// =========================================================================
// Resolve 热点诊断探针（供 bench_resolve 无界面基准使用；默认关闭，零运行时开销）
//
// 此探针统计：
//   remeasureCalls       reMeasureChildren 被调用次数
//   remeasureNodeVisits  所有重测覆盖的子树节点访问总数
//   remeasureMs          重测累计耗时(ms)
// =========================================================================
namespace loppe {
struct BoxModelResolveStats {
    uint64_t remeasureCalls = 0;
    uint64_t remeasureNodeVisits = 0;
    double   remeasureMs = 0.0;
};

LOPPE_API void boxModelResolveStatsEnable(bool enable);
LOPPE_API void boxModelResolveStatsReset();
LOPPE_API BoxModelResolveStats boxModelResolveStatsGet();

// measure 期诊断探针（默认关闭、零运行时开销）：统计 measurePhase 对未变化子树执行的真实测量次数。
struct BoxModelMeasureStats {
    uint64_t cleanRealMeasure = 0;   // 干净(无脏)子树上的真实测量次数
};
LOPPE_API void boxModelMeasureStatsEnable(bool enable);
LOPPE_API void boxModelMeasureStatsReset();
LOPPE_API BoxModelMeasureStats boxModelMeasureStatsGet();
} // namespace loppe