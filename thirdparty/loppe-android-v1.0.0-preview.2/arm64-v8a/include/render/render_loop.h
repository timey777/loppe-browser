/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <memory>
#include <vector>

#include "loppe_export.h"

class Application;
class IDrawer;

namespace loppe {

class Scene;

// RenderLoop —— 渲染流程抽象基类：每帧按固定顺序调用各 on* 步骤，子类覆写关心的步骤即可插入自定义渲染。
class LOPPE_API RenderLoop {
public:
    explicit RenderLoop(::Application& app);
    virtual ~RenderLoop();

    RenderLoop(const RenderLoop&) = delete;
    RenderLoop& operator=(const RenderLoop&) = delete;

    // 模板方法：执行一帧。返回 true 表示本帧发生了 present（已交换缓冲）。
    // 由 Application 主循环每帧调用。
    bool runFrame();

    // 应用退出、图形上下文关闭前调用（用于叠加层清理）。
    virtual void onShutdown();

protected:
    ::Application& app() const;

    // 当前活动场景（可能为 nullptr）。
    Scene* currentScene() const;

    // ---- 每帧步骤（子类可覆写） ----
    // 事件轮询后、处理脏状态前调用（叠加层可在此 newFrame + 构建 UI）。
    virtual void onFrameBegin();
    // 推进动画。
    virtual void onAnimate();
    // 处理待定 resize（节流：每帧至多一次）。
    virtual void onResize();
    // 应用调度中的布局更新。
    virtual void onApplyUpdates();
    // 执行延迟移除的节点。
    virtual void onExecuteRemovals();
    // 返回 true 表示本帧即使没有脏内容也强制出屏（如叠加层需要持续绘制）。
    virtual bool onWantPresent() const;
    // 清屏后、loppe 场景绘制前调用（垫底层渲染点：视频帧/背景）。
    // 绘制内容会保留，作为 loppe 场景的背景；给出绘制器与视口尺寸(DIP)以便绘制。
    virtual void onBeforePaint(IDrawer* drawer, float width, float height);
    // 在 loppe 场景绘制完成后、交换缓冲前调用（叠加层渲染点，如 Dear ImGui）。
    virtual void onFramePresent();
    // 本帧未 present（空闲路径）时调用（叠加层可在此正确结束一帧）。
    virtual void onFrameEnd();
    // 空闲处理（默认 sleep 让出 CPU）。
    virtual void onIdle();

private:
    // 本帧需要合成画面的场景集合（帧内有效）。
    std::vector<Scene*> dirtyScenes_;

    ::Application& app_;
};
// 默认渲染循环（无任何叠加层）。
class LOPPE_API DefaultRenderLoop : public RenderLoop {
public:
    using RenderLoop::RenderLoop;
};

} // namespace loppe