/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#ifndef LOPPE_PIPELINE_GUARD_H
#define LOPPE_PIPELINE_GUARD_H

#include "loppe_export.h"

#include <memory>
#include <string>
#include <typeindex>
#include <vector>

class AbstractEngine;
class ServiceRegistry;
struct ASTNode;

namespace loppe {

class ResolvedTree;

// 管线围栏：逐个执行注册的 engine，单个失败不影响其余，并安排恢复。
class LOPPE_API PipelineGuard {
public:
    // tree：失败时要做「整树失效」的那一棵解析树（可空）
    explicit PipelineGuard(ResolvedTree* tree = nullptr);

    void attachTree(ResolvedTree* tree);

    // 跑一遍管线：order == nullptr 表示按注册顺序。
    // 返回 true = 全部 engine 正常跑完；false = 至少一个 engine 抛了。
    bool runEngines(ServiceRegistry& registry,
                    const std::vector<std::type_index>* order,
                    const std::shared_ptr<ASTNode>& ast,
                    float viewportW,
                    float viewportH);

    // 报告一次失败：记日志 + 整树失效 + 安排一次全量重来（连续次数有上限）
    void reportFailure(const char* phase, const std::string& detail);

    // 取走「上一趟是否有失败」，取走即清
    bool consumeFailure();

    // 连续重来次数上限（默认 2）
    void setRetryBudget(int budget);

private:
    void invalidateTree();
    void requestFullRedo();

    static constexpr int kDefaultRetryBudget = 2;

    ResolvedTree* tree_ = nullptr;
    int  retryBudgetLeft_ = kDefaultRetryBudget;
    bool gaveUp_   = false;
    bool failed_   = false;
};

} // namespace loppe

#endif // LOPPE_PIPELINE_GUARD_H
