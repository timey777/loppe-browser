/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// element_view.h — 元素视图句柄（类 DOM 的通用操作入口）

#include <memory>
#include <string>

#include "loppe_export.h"

namespace loppe {

class AbstractControl;

class LOPPE_API ElementView {
public:
    ElementView() = default;
    explicit ElementView(std::shared_ptr<AbstractControl> c);

    // 由 UIEvent::target（裸控件指针）取句柄；拿不到 shared_ptr 的控件（如尚未挂载）返回空句柄。
    static ElementView fromControl(AbstractControl* c);

    bool valid() const;
    explicit operator bool() const;

    // 底层控件（可能为空；业务属性/专用操作从这里取回具体类型后自己做）
    AbstractControl* control() const;

    // 标识（句柄失效时返回空串）
    std::string id() const;
    std::string uid() const;

    // ── 样式类（CSS class，通用）──
    ElementView& addClass(const std::string& cls);
    ElementView& removeClass(const std::string& cls);
    bool hasClass(const std::string& cls) const;

    // ── 结构 / 查询 ──
    // 从父容器摘除；当帧起布局/命中不再包含它。
    void remove();
    // 祖先/后代查询（按 class）。ancestorByClass 含自身。
    // 例：ElementView::fromControl(e.target).ancestorByClass("row")
    ElementView ancestorByClass(const std::string& cls) const;
    ElementView descendantByClass(const std::string& cls) const;

private:
    std::weak_ptr<AbstractControl> ctrl_;
};

} // namespace loppe
