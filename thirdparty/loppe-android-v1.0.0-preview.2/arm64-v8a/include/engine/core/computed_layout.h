/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "render/display_list.h"
#include "node_flags.h"
#include "misc/measure_cache.h"
#include <string>
#include <cfloat>

namespace loppe {

	// 布局算法配置（从 LayoutEngine 的 @layout 块获取）
	struct CompiledLayoutRules
	{
		// 布局算法类型
		FormattingContext displayEnum = FormattingContext::Block;

		void setDisplay(FormattingContext display);

		FormattingContext getDisplay() const;

		Position position = Position::Static;

		// Flex 布局属性
		FlexDirection flexDirection = FlexDirection::Row;
		AlignItems alignItems = AlignItems::Stretch;
		AlignSelf alignSelf = AlignSelf::Auto;
		JustifyContent justifyContent = JustifyContent::FlexStart;
		FlexWrap flexWrap = FlexWrap::NoWrap;
		float flexGrow = 0;
		float flexShrink = 0;

		// Grid 布局属性
		std::string gridTemplateColumns = "";
		std::string gridTemplateRows = "";
		float gap = 0;

		// 逐字段比较两个布局算法配置是否相等。
		bool equals(const CompiledLayoutRules& o) const;

	private:
		// Deprecated string version, mapping to displayEnum
		std::string display = "block";
	};

	// 计算后的布局数据
	struct ComputedLayout
	{
		// 盒模型尺寸 (Margins & Paddings & Borders)
		ui::Edges margin;
		ui::Edges padding;
		ui::Edges border;

		// flags indicating the corresponding margin was specified as 'auto'
		bool marginTopAuto = false, marginRightAuto = false, marginBottomAuto = false, marginLeftAuto = false;

		float minWidth = 0, minHeight = 0;
		float maxWidth = FLT_MAX, maxHeight = FLT_MAX;

		// 最终计算的盒子（像素值）contentBox(相对坐标), paddingBox(相对坐标), borderBox(绝对坐标), marginBox(绝对坐标)
	private:
		loppe::ui::Rect contentBox_; // 内容区域 (Relative: 相对于 borderBox 左上角的偏移)
		loppe::ui::Rect paddingBox_; // 包含内边距 (Relative: 相对于 borderBox 左上角的偏移)
		loppe::ui::Rect borderBox_;  // 包含边框 (Absolute: 相对于根/窗口的绝对坐标)
		loppe::ui::Rect marginBox_;  // 包含外边距 (Absolute: 相对于根/窗口的绝对坐标)
	public:
		// Accessors (Read-only for consumers)
		const loppe::ui::Rect &getContentBox() const;
		const loppe::ui::Rect &getPaddingBox() const;
		const loppe::ui::Rect &getBorderBox() const;
		const loppe::ui::Rect &getMarginBox() const;
		loppe::ui::Rect getAbsolutePaddingBox() const;
		loppe::ui::Rect getAbsoluteContentBox() const;

		// Internal calculation
		void finalizeSizes();
		void setPosition(float absX, float absY);

		// 实际使用的尺寸（auto 时由布局计算得出）。
		float computedWidth = 0;
		float computedHeight = 0;
		// 内在（内容）尺寸，独立于最终尺寸（用于 scroll 范围 / 调试）。
		float intrinsicWidth = 0;
		float intrinsicHeight = 0;
		// 主轴 flex 基准：测量定稿时的内容尺寸（不含 grow/shrink 分配的结果）。
		float flexBaseWidth = 0;
		float flexBaseHeight = 0;
		// 文字绘制
		bool wrap = false;

		// 测量缓存（内部使用）。
		MeasureCache measureCache;

		/* ================== Scroll Viewport（滚动视口） ================== */

		// 可视区域大小（初始 = paddingBox.size，滚动条出现后会扣除滚动条尺寸）
		float viewportWidth = 0;
		float viewportHeight = 0;

		/* ================== 逐轴裁剪（overflow-x / overflow-y） ================== */
		// 该轴为 visible 时不裁剪，也不出该轴滚动条。
		bool clipX = true;
		bool clipY = true;

		/* ================== 内容边界（绝对坐标） ================== */
		// 本节点内容的最右/最下边：默认 = 自身 marginBox 边界；
		// 该轴 overflow=visible 时并进子孙的溢出 → 父级算滚动范围时一并算上
		//（CSS 语义：visible 的溢出会一路传播到最近的滚动容器）。
		float contentExtentRight = 0;
		float contentExtentBottom = 0;

		/* ================== Scroll 状态 ================== */
		struct ScrollData
		{
			float offsetX = 0, offsetY = 0;
			float scrollWidth = 0, scrollHeight = 0;
		};

		// overflow != "visible" 且内容超出 viewport 时使用
		ScrollData scroll;
	};

} // namespace loppe