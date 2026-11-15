/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
#include <memory>
#include <stack>
#include <string>
#include <vector>
#include "components/scene.h"
#include "engine/core/coordinate_types.h"
#include "engine/platform/platform_host.h"
#include "components/uitypes.h"
#include "loppe_export.h"

namespace loppe {

/**
 * @brief 舞台 - 持有一个场景栈，并回答「输入/渲染该找哪个场景」
 *
 * 单 Stage 用法（默认）：Application 内部建的场景挂在 getInstance() 上，无需关心本类。
 *
 * 多 Stage 用法（一个窗口装多份界面，如浏览器壳 + 每个标签页一份）：
 *     Stage tabStage;                        // 每份界面一个 Stage
 *     tabStage.replaceScene(scene);          // 场景栈用法与默认 Stage 完全一致
 *     tabStage.setViewRect({0, 40, 800, 760});    // 场景画在窗口客户区的哪块（DIP，窗口坐标）
 *     app.views().attach(&tabStage, &tabCompositor, &tabHost);   // 注册进窗口的视图槽
 *
 * 注册后的行为：
 *   - 鼠标事件（按下/移动/滚轮）按 viewRect 命中路由：点在谁的区域里就派发给谁，
 *     场景收到的坐标已平移成局部坐标；点不进任何附加 Stage → 给默认 Stage（壳）。
 *   - 键盘/字符事件给 active() 指向的 Stage（键盘焦点归属，宿主用 setActive 切换）。
 *   - 绘制时壳先画满窗口，各附加 Stage 按自己的 viewRect 叠画在其上（各自 Compositor）。
 */
class LOPPE_API Stage {
public:
    Stage();
    ~Stage();

    // 不可拷贝：Stage 与一块窗口区域、一个平台宿主绑定，需要引用语义。
    Stage(const Stage&) = delete;
    Stage& operator=(const Stage&) = delete;

    // 默认 Stage（单例）：Application 内部那套（壳场景）挂在这里。向后兼容入口。
    static Stage& getInstance();

    // 当前进程内所有活着的 Stage（含默认单例）。用于需要遍历「全部界面」的场合，
    // 例如按可见性推进动画；表由 Stage 自行维护，不要保存它的引用。
    static const std::vector<Stage*>& liveStages();

    // 键盘焦点的归属：当前活动 Stage（默认 = 默认 Stage；永不为 null）。
    static Stage* active();
    // 切换键盘焦点所属的 Stage；传 nullptr 回落到默认 Stage。
    static void setActive(Stage* stage);

    void pushScene(std::shared_ptr<Scene> scene);
    void replaceScene(std::shared_ptr<Scene> scene);
    void popScene();

    // 显式释放所有场景（有序关停用）：Scene 析构会连带销毁其持有的 runtime/解析树。
    void clearScenes();

    std::shared_ptr<Scene> currentScene() const;

    // 进程级平台能力宿主（剪贴板/光标/文件选择等）。多 Stage 共享同一窗口时，
    // 各 Stage 应指向同一个 IPlatformHost 实现。
    void setPlatformHost(IPlatformHost* h);
    IPlatformHost* getPlatformHost() const;

    // 本 Stage 的场景在窗口客户区里的矩形（DIP，窗口坐标，原点 = 客户区左上角）。
    // 设置后（hasViewRect() == true）：鼠标事件进入场景前平移掉矩形原点、绘制时场景
    // 画在该矩形内（超出裁剪）；未设置 = 占满整个窗口、不平移。
    void setViewRect(const ui::Rect& r);
    const ui::Rect& viewRect() const;
    bool hasViewRect() const;

    // 窗口逻辑坐标（DIP）→ 本 Stage 的场景局部坐标：减去 viewRect 原点。
    // 未设 viewRect（占满窗口）时原样返回。只做原点平移，不含滚动/visualOffset。
    LogicalPointDp toLocalPoint(const LogicalPointDp& p) const;

    // 是否参与绘制与鼠标命中（默认 true）。切走的标签页等「不在前台的界面」置 false：
    // 不画、也不接收鼠标（键盘由 active() 决定，与可见性无关）。
    void setVisible(bool visible);
    bool isVisible() const;

private:
    std::stack<std::shared_ptr<Scene>> scenes_;
    IPlatformHost* m_platformHost = nullptr;
    ui::Rect m_viewRect{};
    bool m_hasViewRect = false;
    bool m_visible = true;

    static Stage* s_active;
};

} // namespace loppe
