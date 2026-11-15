/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "abstract_control.h"

namespace loppe {

/**
 * @brief 进度条控件
 *
 * 轨道 = 控件自身背景（CSS 的 background-color / border-radius）；填充 = barColor()，
 * 宽度 = progress × 轨道宽，圆角收敛到「半宽 / 半高」以内。
 *
 * 用法：
 *   DSL：`progress:su_bar(class="bar") {}` —— class 里给 width/height/background-color；
 *        可选 `value="0.68"`（0~1 的取值，超出按 0/1 裁剪）给出**声明取值**，界面一开始就是这个比例；
 *        不写 `value` 时进度由宿主代码驱动
 *   代码：`bar->setProgress(0..1)` / `bar->setBarColor(color)`（从 ControlManager 按 id 取控件）
 *
 * 线程：控件接口只允许 UI 线程调用（与所有控件一致）；进度数据由调用方跨线程传递
 * （后台线程 post 到引擎 `UiDispatcher`，UI 线程在任务里再 setProgress）。
 */
class LOPPE_API ProgressBar : public AbstractControl {
protected:
    // 禁止外部直接构造，使用 ControlFactory 创建（DSL 或 CreateProgressBar()）
    ProgressBar() = default;
    friend class ControlFactory;

    // 子类钩子：读 AST 的 `value`，把声明取值应用成进度（取值变化时才应用）
    void OnSyncFromResolvedNode(const ResolvedNode& rn, ControlSync mask) override;

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;

public:
    ~ProgressBar() override = default;

    std::string getTypeName() const override;

    // 进度：0..1（超范围自动 clamp；NaN 当 0）。
    void setProgress(float progress);
    float getProgress() const;

    // 填充色（默认蓝）。
    void setBarColor(const ui::Color& color);
    const ui::Color& getBarColor() const;

private:
    float progress_ = 0.0f;
    ui::Color barColor_{ 0.23f, 0.51f, 0.96f, 1.0f };

    float declaredProgress_ = 0.0f;
    bool hasDeclaredProgress_ = false;
};

} // namespace loppe
