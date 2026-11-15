/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// ═══════════════════════════════════════════════════════════════════
// component_host.h — 受管组件宿主（组件模式的推荐实现）
//
// 用法：Mount() 一次性挂载模板（含 @style/@layout，Local 规则只注册一次）→
//       SetRows() 按稳定 key 对账行集合 → Unmount() 一次移除本宿主挂载的全部内容。
//
// 约束：
//   · 行 DSL 不得含 @style/@layout 块；行内引用的 class 由 Mount 的模板 CSS 提供，
//     行节点共享模板的 Local 作用域（不重复注册 CSS）。
//   · SetRows 的 containerId 必须是 Mount 模板里带 id 的内容节点（专属行容器）。
// ═══════════════════════════════════════════════════════════════════

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "loppe_export.h"

class DocumentRuntime;

namespace loppe {

class Scene;
class ResolvedNode;
class AbstractControl;
class ContainerControl;

// 行 = 稳定 key + 内容 DSL（引用宿主模板 CSS 里的 class，不含 @style/@layout）
struct LOPPE_API ComponentRow {
    std::string key;
    std::string dsl;
};

class LOPPE_API ComponentHost {
public:
    ComponentHost() = default;
    ~ComponentHost();  // 未显式 Unmount 时自动卸载

    ComponentHost(const ComponentHost&) = delete;
    ComponentHost& operator=(const ComponentHost&) = delete;

    // 一次性挂载模板：templateDsl 顶层内容节点成为 parentId 的子节点。
    bool Mount(Scene* scene, const std::string& parentId, const std::string& templateDsl);

    // 使 containerId 下的行集合等于 items（同 key 复用；缺失 key 的行隐藏）。
    bool SetRows(const std::string& containerId, const std::vector<ComponentRow>& items);

    // 移除本宿主全部已挂载内容（含隐藏池行），立即落地。
    void Unmount();

    bool isMounted() const;
    Scene* getScene() const;

private:
    struct RowEntry {
        std::string key;
        std::string dsl;
        ResolvedNode* root = nullptr;              // 行根 resolved 节点（容器子节点）
        std::shared_ptr<AbstractControl> ctrl;     // 行根控件（显隐/顺序用）
        bool hidden = false;
    };
    struct ContainerEntry {
        ResolvedNode* node = nullptr;              // 容器 resolved 节点
        std::shared_ptr<AbstractControl> ctrl;     // 容器控件
        std::unordered_map<std::string, RowEntry> rows;
    };

    bool ensureContainer(const std::string& containerId, ContainerEntry& out);
    bool mountRow(ContainerEntry& c, const ComponentRow& item);
    void replaceRow(ContainerEntry& c, const std::string& key, const ComponentRow& item);
    void setRowHidden(ContainerEntry& c, const std::string& key, bool hidden);
    void attachResolvedContent(ResolvedNode* parent,
                               const std::vector<ResolvedNode*>& contents,
                               std::vector<ResolvedNode*>& attachedOut);
    void detachResolved(ResolvedNode* node);  // 从父/ast/控件摘除并释放（单行物理移除）

    Scene* scene_ = nullptr;
    DocumentRuntime* ctx_ = nullptr;
    int scopeId_ = -1;                                  // Mount 模板的 Local 作用域 id
    bool mounted_ = false;
    std::vector<ResolvedNode*> templateRoots_;          // 模板顶层内容 resolved 节点
    std::vector<std::shared_ptr<AbstractControl>> templateCtrls_;
    std::unordered_map<std::string, ContainerEntry> containers_;  // containerId → 行状态
};

} // namespace loppe
