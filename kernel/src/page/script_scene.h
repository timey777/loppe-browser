// script_scene.h —— browser 侧的 Scene 基类。
//
// 分工（也是引擎的边界）：
//   引擎 Scene 只做通用机制（派发、焦点、命中）；
//   「给谁绑什么事件」这类宿主糖长在这里，页面场景（PageScene）从这里继承。
//   要加新的事件/宿主行为时改这个类，不动引擎。
//
// 便捷绑定都是 UIEventSystem::bind 的薄封装；selector 语法同引擎（"#id" / ".class"）。

#pragma once

#include <string>
#include <utility>
#include "components/scene.h"

class ScriptScene : public loppe::Scene {
public:
    using Scene::Scene;

    // 指针按键（右键也发；UIEvent::button 区分按键，UIEvent::mods 带修饰键）
    void onMouseDown(const std::string& selector, loppe::UIEventSystem::EventCallback cb)
    {
        getUIEventSystem()->bind(selector, loppe::UIEventSystem::UIEventType::MouseDown, std::move(cb));
    }
    void onMouseUp(const std::string& selector, loppe::UIEventSystem::EventCallback cb)
    {
        getUIEventSystem()->bind(selector, loppe::UIEventSystem::UIEventType::MouseUp, std::move(cb));
    }
    // 键盘（派发给焦点控件及其祖先；没有焦点控件时不派发）
    void onKeyDown(const std::string& selector, loppe::UIEventSystem::EventCallback cb)
    {
        getUIEventSystem()->bind(selector, loppe::UIEventSystem::UIEventType::KeyDown, std::move(cb));
    }
    void onKeyUp(const std::string& selector, loppe::UIEventSystem::EventCallback cb)
    {
        getUIEventSystem()->bind(selector, loppe::UIEventSystem::UIEventType::KeyUp, std::move(cb));
    }
    // 焦点进出（target = 拿到/失去焦点的那个控件）
    void onFocus(const std::string& selector, loppe::UIEventSystem::EventCallback cb)
    {
        getUIEventSystem()->bind(selector, loppe::UIEventSystem::UIEventType::Focus, std::move(cb));
    }
    void onBlur(const std::string& selector, loppe::UIEventSystem::EventCallback cb)
    {
        getUIEventSystem()->bind(selector, loppe::UIEventSystem::UIEventType::Blur, std::move(cb));
    }
};
