/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "engine.h"
#include "engine/core/parse.h"
#include <memory>

class ASTBuildEngine : public AbstractEngine {
public:
    ASTBuildEngine() = default;
    ~ASTBuildEngine() = default;

    void initialize() override;
    void shutdown() override;

    std::type_index type() const override;
    // 由 AST 构建或更新解析树（ResolvedTree）。
    void process(std::shared_ptr<ASTNode> root, loppe::DisplayList& outList, float viewportW, float viewportH) override;

    void onASTUpdated(std::shared_ptr<ASTNode> root) override;

    std::string name() const override;

private:
    std::shared_ptr<ASTNode> lastAST_;
};