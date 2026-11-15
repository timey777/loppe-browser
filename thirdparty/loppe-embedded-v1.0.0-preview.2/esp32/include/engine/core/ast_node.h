/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "attribute.h"
#include "loppe_export.h"
#include "misc/stable_id.h"
#include <optional>
// ------------------- AST & Visitor -------------------
struct LOPPE_API ASTNode {
public:
    std::unordered_map<std::string, Attribute> attrs;
    std::optional<int> localScopeId;
public:
    std::string type; // e.g. "Root", "Import", "Panel", "StyleRule"
    // 解析时记录的“内容节点”声明起始行（1-based；0=未记录）。
    // 仅内容节点（ContentParser 产生）填写；@style/@layout 等 meta 规则不填。
    int srcLine = 0;
    // 该节点在源码里的**起始字节偏移**（1-based；0=不是从源码解析出来的）。
    std::size_t srcStart = 0;
    // 该节点在源码里的**结束字节偏移**（开区间；0=未记录）。
    std::size_t srcEnd = 0;
    // 节点身份（整数），创建后不变。
    loppe::NodeUid uid = loppe::kUidNone;
    std::vector<std::shared_ptr<ASTNode>> children;
    // 存放解析时分离出的引擎注解块，例如 @style/@layout/@script 等
    // key 通常为原始关键字（例如 "@style"），value 列表为对应解析出来的 AST 节点
    std::unordered_map<std::string, std::vector<std::shared_ptr<ASTNode>>> meta;

    // 父节点指针（不拥有所有权）。
    std::weak_ptr<ASTNode> parent;

    // 支持 classList
    std::vector<std::string> classList;

    void setClassList(const std::vector<std::string>& classes);
    const std::vector<std::string>& getClassList() const;
    // helper factory
    static std::shared_ptr<ASTNode> make(const std::string& t);
    ASTNode() = default;
    explicit ASTNode(const std::string& t);

    // 按需将另一个 AST 的内容合并到当前节点（就地更新，尽量保留原节点指针）
    void updateFrom(const std::shared_ptr<ASTNode>& other);

    // 将节点加入 meta 存储（用于顶层 Root 节点保存多个引擎块）
    void addMeta(const std::string& key, std::shared_ptr<ASTNode> node);

    // 获取 meta 条目（若不存在返回 nullptr）
    const std::vector<std::shared_ptr<ASTNode>>* getMeta(const std::string& key) const;

    bool hasMeta(const std::string& key) const;

    // 获取属性对象
    const Attribute* getAttribute(const std::string& key) const;

    // 检查是否有某个属性
    bool hasAttribute(const std::string& key) const;

    bool setAttribute(const std::string& key, const Attribute& attr);

    // 判断是否有某个 class（用于样式选择器）
    bool hasClass(const std::string& cls) const;
    void setLocalScopeId(std::optional<int> id);
    const std::optional<int> getLocalScopeId();
};