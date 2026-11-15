/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
// soft_keyboard.h — 引擎内置软键盘（触摸屏文本输入）
//
// 引擎内置的屏幕底部键盘面板，绘制在界面最上层；键位表为「字母 + 数字符号」两层，
// 键帽字体可用 setFontFamily 指定。
//
// 调用约定：
//   · 焦点进 Input（输入源为 Touch）自动弹出、焦点离开自动收起；桌面（鼠标输入源）
//     不自动弹，用 show()/hide() 手动控制。
//   · 键盘可见时落在键盘面板上的指针事件被吃掉，不会穿透到底下的界面。
//   · 按键直接对焦点 Input 出字（onChar / onKey），宿主零接线。
//   · 屏幕逻辑尺寸经 Scene::setSoftKeyboardViewport(w, h) 自动同步。

#include <string>

#include "components/abstract_control.h"

namespace loppe {

class Input;
class Scene;

class LOPPE_API SoftKeyboard : public AbstractControl {
public:
    // 键位表条目（公开：无头用例直接对着键位表做命中/出键断言）
    struct KeySpec {
        const char* id;
        const char* payload;   // 出什么字（UTF-8；空格等可与 label 不同）
        const char* label;     // 键帽显示
        const char* group;     // alpha / num / always
        float x, y, w, h;      // 归一化矩形（相对键盘面板）
        int kind;              // 0=Char 1=Back 2=Shift 3=Layer 4=Enter
    };

    SoftKeyboard();

    // ---- 开关 / 状态 ----
    void setEnabled(bool on) override;
    bool isEnabled() const;

    // 自动弹出（默认开）：焦点进输入框弹、出输入框收。关掉后只听 show()/hide()。
    void setAutoShow(bool on);
    bool autoShow() const;

    void show();     // 手动显示（期间自动「收起」不生效，直到 hide()）
    void hide();     // 收起（清 ⇧ / ?123 / 按住状态）
    bool isVisible() const;

    // 焦点驱动的自动弹出（Scene 在焦点真正切换后调；事件驱动，不轮询）：
    // 焦点进输入框 → 显示，离开 → 收起。受 enabled / autoShow / 手动 show() 约束。
    void setInputFocused(bool focused);

    // 屏幕逻辑尺寸（键盘贴底、宽度铺满；ViewCoordinator 的 viewport 变化会自动同步）
    void setScreenSize(float w, float h);
    float panelHeight() const;   // 随屏幕宽度缩放，最多占屏幕高度的 45%

    // 键帽字体（空 = 平台默认）；键帽字号由面板高度推算
    void setFontFamily(const std::string& family);
    const std::string& fontFamily() const;

    // ---- 指针（屏幕 dp 坐标；Scene 在命中测试前转发）----
    // down：落在**键盘面板**上就消费并返回 true（键间空隙也算，绝不漏给底层界面）；
    //       真正落在某个键上才出字。按住期间 move/up 全部消费。
    bool pointerDown(float x, float y);
    bool pointerMove(float x, float y);
    bool pointerUp(float x, float y);
    bool engaged() const;

    // ---- AbstractControl ----
    void update() override;
    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;
    std::string getTypeName() const override;

    // 命中（屏幕 dp 坐标）：点是否落在键盘面板内
    bool containsPoint(float x, float y) const;

private:
    const KeySpec* hitTest(float x, float y) const;
    void invalidateKey(const KeySpec* k);
    void activate(const KeySpec& k);
    std::string labelOf(const KeySpec& k) const;
    const char* payloadOf(const KeySpec& k) const;
    ui::Rect keyRect(const KeySpec& k) const;
    ui::Color faceOf(const KeySpec& k) const;

    Input* focusedInput() const;
    void deliverChar(const char* utf8);
    void deliverBackspace();

    bool enabled_ = true;
    bool autoShow_ = true;
    bool manualOverride_ = false;   // show() 之后为真：自动逻辑不收起，直到 hide()
    bool visible_ = false;
    bool engaged_ = false;          // 手指正按在键盘上

    bool caps_ = false;             // ⇧ 之后打一个字自动回小写
    bool sym_ = false;              // 在「?123」层

    float screenW_ = 240.f;
    float screenH_ = 320.f;

    std::string fontFamily_;

    const KeySpec* pressed_ = nullptr;
    double pressAtSec_ = 0.0;
    double lastRepeatSec_ = 0.0;
    bool tickPending_ = false;      // update() 里决定下一帧是否继续 tick
};

} // namespace loppe
