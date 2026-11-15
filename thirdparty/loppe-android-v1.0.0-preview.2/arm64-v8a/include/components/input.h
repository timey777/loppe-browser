/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once
#include "abstract_control.h"
#include "render/display_list.h"
#include "scene.h"
#include "render/text_measurer.h"
#include "engine/event/ui_event_system.h"
#include "render/drawer.h"
#include "control_factory.h"
#include "render/text_layout.h"
#include "misc/utf8util.h"
#include "engine/update_scheduler.h"
#include "misc/editor_undo.h"
#include "input/text_buffer.h"
#include "input/text_selection.h"
#include "input/caret_blink.h"
#include "input/key_repeat.h"
#include "input/text_edit_ops.h"
#include "text/multi_click_select.h"
#include "text/text_searcher.h"
#include "text/text_segment.h"
#include <string>
#include <utility>
#include <vector>
#include <chrono>
#include "engine/event/input_events.h"
#include "engine/core/resolved_node.h"
#include <limits>
#include <cmath>
#include <algorithm>
#include <cassert>

namespace loppe {

/**
 * @brief 文本输入控件（含单行/多行、选区、撤销重做、自动换行、滚动）
 *
 * 鼠标：单击定位光标；双击 = 选中词段；三击 = 选中整逻辑行（单行输入即全文）；
 * 双击/三击后拖拽按词/行扩选。
 *
 * 滚动条：**仅多行（textarea）才有**——纵向=内容高于视口；横向=wrap=false 且宽于视口。
 * 单行输入不显示任何滚动条（同浏览器 convention），超宽文本靠光标跟随内部平移。
 *
 * 密码模式：`setType("password")`（DSL `type="password"`）后显示按码点遮成 '*'；
 * 取值/复制/事件仍是真实文本。
 */
class LOPPE_API Input : public AbstractControl {
public:
    Input();
    explicit Input(const std::string& value);

    // 直连文本测量器（无 Scene 的最小宿主使用；有 Scene 时仍优先走 Scene 的测量器）
    void setTextMeasurerOverride(ITextMeasurer* m);

    void onRender(DisplayList& displayList, const ui::Rect& clip) const override;
    std::string getTypeName() const override;

    void setValue(const std::string& v, bool notify = true);
    const std::string& getValue() const;

    // 只读：当前光标/选区（codepoint 索引）
    const TextSelection& selection() const;

    // 全选（选区 = 全文）
    void selectAll();

    // 查找状态（查询串、命中集、当前命中项）：本控件按 ranges() 画命中高亮，
    // 并把当前命中落成自己的选区。
    TextSearcher& textSearcher();

    // 滚动条可见性（只有多行才有；单行输入恒 false——超宽文本靠光标跟随平动）
    bool hasVScrollbar() const;
    bool hasHScrollbar() const;

    void setPlaceholder(const std::string& p);
    const std::string& getPlaceholder() const;

    void setType(const std::string& t);   // "Input" 单行 / "textarea" 多行 / "password" 密码遮盖
    const std::string& getType() const;

    void setWrap(bool wrap);
    bool getWrap() const;

    // 文本颜色
    void setTextColor(const ui::Color& color);
    const ui::Color& getTextColor() const;

    void setFillColor(const ui::Color& color) override;

    void setFontSize(float size);
    float getFontSize() const;

    void setFontFamily(const std::string& family);
    const std::string& getFontFamily() const;

    void OnSyncFromResolvedNode(const ResolvedNode& rn, ControlSync mask) override;

    void setPadding(float left, float top, float right, float bottom) override;

    void setScene(Scene* scene) override;

    // 滚动尺寸由本控件自行计算，忽略外部传入值。
    void setScrollSize(float w, float h) override;

private:
    // 无 Scene 最小宿主直连的文本测量器（可选）
    ITextMeasurer* textMeasurerOverride_ = nullptr;

    struct TextLayoutLocalPoint {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct TextRenderGeometry {
        float innerWidth = 0.0f;
        float lineHeight = 0.0f;
        ui::Rect contentClipRect;
        float contentHeight = 0.0f;
        float totalContentHeight = 0.0f;
        float maxScrollY = 0.0f;
        float currentScrollY = 0.0f;
        float maxScrollX = 0.0f;
        float currentScrollX = 0.0f;
        float textOriginX = 0.0f;
        float textOriginY = 0.0f;
    };

    // ---- 共享文本模型 ----
    TextBuffer buffer_;   // UTF-8 文本内容（codepoint 编辑单位 + 行索引）
    TextSelection sel_;   // 光标 + 选区（codepoint 索引）
    // 撤销/重做（状态捕获/应用钩子；钩子在构造函数里接线）
    EditorUndo undo_;
    std::string placeholder_;
    std::string type_ = "Input";
    bool multiline_ = false;
    bool passwordMode_ = false;   // type_ == "password"：显示遮 '*'（真实文本照常存取）
    bool wrap_ = false;  // 多行时是否自动换行（显示换行，不改变文本字符串）
    ui::Color textColor_{0.0f, 0.0f, 0.0f, 0.0f};
    ui::Color caretColor_{0, 0, 0, 1};
    ui::Color lineBorderColor_{0, 0, 0, 0.2f};
    std::string fontFamily_;
    float fontSize_ = 0.0f;  // 0表示使用默认大小

    // Insert 键切换：false=插入模式（默认），true=覆盖模式
    bool overwriteMode_ = false;
    // 光标闪烁状态机
    CaretBlink blink_;
    // 长按退格/删除重复状态机
    KeyRepeat backspaceRepeat_;
    KeyRepeat deleteRepeat_;
    bool selecting_ = false; // 鼠标拖拽中
    bool selectingByShift_ = false;
    // 最近一次拖拽指针位置（control-local）
    float lastDragX_ = 0.0f;
    float lastDragY_ = 0.0f;
    TextSearcher textSearcher_;

    // 双击/三击判定与拖拽粒度（单击 Char；双击后拖拽按词、三击后按行）
    textsel::MultiClickSelect multiClick_;

    // 把滚动偏移钳回可滚动范围
    void clampScrollToContent();

    // 拖拽框选自动滚动
    bool tickSelectionAutoScroll();

    // overlay scrollbar instances (created lazily)
    mutable std::shared_ptr<ScrollbarControl> vScrollbar_;
    mutable std::shared_ptr<ScrollbarControl> hScrollbar_;

    // Layout cache
    mutable std::shared_ptr<ITextLayout> textLayout_;
    mutable bool layoutDirty_ = true;
    float lastLayoutInnerWidth_ = -1.0f;
    float contentHeight_ = 0.0f;
    float totalContentHeight_ = 0.0f;
    float totalContentWidth_ = 0.0f;
    float lineHeight_ = 0.0f;
    std::vector<render::Rect> selectionRects_;
    bool selectionDirty_ = true;
    // Remember desired X position when moving vertically
    float preferredCursorX_ = -1.0f;

    float getInnerTextWidth() const;
    float getResolvedLineHeight(float fontSize) const;
    float getTextLocalStartX() const;
    float getTextLocalStartY() const;
    TextRenderGeometry getTextRenderGeometry(float fontSize) const;
    TextLayoutLocalPoint toTextLayoutLocalPoint(ControlLocalPointDp point) const;
    int getCharIndexAtControlPoint(ControlLocalPointDp point) const;
    int dragHitCp(float x, float y) const;

    void updateCaretColor();
    void updateLineBorderColor();

    void ensureScrollbarCreated();
    void updateScrollbarVisibility();

    void requestLayoutRebuild();

    // 语义刷新辅助（update 驱动 vs 纯重绘）
    void requestControlUpdate();
    void requestVisualRepaint();

    bool canIncrementallyEditLayout() const;

    void afterLocalEdit(int startCp, int delLenCp, const std::string& insertedUtf8);

    void refreshDerivedMetricsAfterIncremental();

    void editRangeForInsert(const std::string& insertedUtf8, int* outStartCp, int* outDelLenCp);
    void editRangeForDelete(bool deletePrevious, int* outStartCp, int* outDelLenCp);

    bool isInRenderPhaseChangingLayout() const;

    static const Input*& activeRenderInput();

    void refreshSelectionRectsCache();

    // 选区变更后调用：标记脏并重建矩形缓存
    void markSelectionDirty();

    void clearSelection();

public:
    void updateTextLayout();

    // 布局同步完成（TreeSynchronizer 调用）：按新 bounds 重建文本排版
    void onLayoutSynced() override;

    // 帧内收尾：补建本帧内标脏的内部排版。
    void onFrameCommit() override;

    // ---- 撤销/重做：捕获 / 应用 ----

    EditorUndo::EditState captureState() const;

    void applyState(const EditorUndo::EditState& s);

    // ---- 编辑后统一收尾：重建布局 + 重绘 + 事件 ----
    void afterEdit();

    // 确保光标行可见。
    void ensureCaretVisible();
    // 文字区视口高度（去上下内边距）；<= 0 表示视口尚未落地（隐藏面板/首次布局未完成）
    float textViewportHeight() const;

    // 事件处理
    void onFocus() override;
    void onBlur() override;

    // 每帧更新（由 Scene 调度）
    void update() override;

    bool handleLocalMouseButton(const ControlLocalMouseButtonEvent& e) override;
    bool handleLocalMouseMove(const ControlLocalMouseMoveEvent& e) override;
    bool cursorIntent(const ControlLocalPointDp& point, CursorIntent& out) const override;

    bool onKey(const KeyEvent& e) override;
    bool onChar(const CharEvent& e) override;

    bool handleLocalScroll(const ControlLocalScrollEvent& e) override;

private:
    bool inputEventPending_ = false;
    bool caretVisibilityPending_ = false;
    bool viewportValid_ = false;

    // Dispatch Input event to UIEventSystem when value changes
    void emitInputEvent();

    void emitScrollEvent();
    // 查找接线：给 TextSearcher 装 textSource / 命中落选区两个钩子
    void wireTextSearcher();
    // 把当前命中项落进本控件的选区
    void applyFindSelection(int startCp, int endCp);
};

} // namespace loppe
