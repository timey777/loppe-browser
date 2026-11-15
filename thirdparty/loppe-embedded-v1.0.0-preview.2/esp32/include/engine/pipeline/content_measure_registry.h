/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

// content_measure_registry.h — 节点固有内容尺寸测量注册表
//
// 内置类型（image/text/input/combobox）已注册；自定义控件可用 registerType() 注册新类型策略。
#pragma once

#include <functional>
#include <map>
#include <string>

namespace loppe {

class ResolvedNode;
class ITextMeasurer;

class ContentMeasureRegistry {
public:
    // 测量上下文（布局时注入）
    struct Context {
        ITextMeasurer* textMeasurer = nullptr;
    };

    // 策略：测量节点固有内容尺寸，仅填充仍为 AUTO 的 requestedW/requestedH。
    // requestedW/requestedH 为 in/out；availableWidth 为父级可用宽度（换行测量用）。
    using Strategy = std::function<void(ResolvedNode* node, const Context& ctx,
                                        float& requestedW, float& requestedH, float availableWidth)>;

    static ContentMeasureRegistry& instance();

    // 按类型注册测量策略（重复注册覆盖）
    void registerType(const std::string& type, Strategy strategy);

    // 按节点类型分派测量；未注册类型返回 false（无固有尺寸）
    bool measure(const std::string& type, ResolvedNode* node, const Context& ctx,
                 float& requestedW, float& requestedH, float availableWidth) const;

private:
    ContentMeasureRegistry();  // 注册内置策略
    std::map<std::string, Strategy> strategies_;
};

} // namespace loppe