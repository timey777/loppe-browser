/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

namespace loppe
{
	// 光标意愿：控件声明「指针在我的这个点上该是什么光标」，由 Scene 统一裁决后下发。
	// type 取 Scene::CursorType 的数值。
	struct CursorIntent
	{
		int type = 0;
		int priority = 0;
	};

	// 优先级：数值大者胜；同优先级时内层（更具体的控件）胜。
	// 用法：cursorIntent() 里填 cursor_prio::kText 之类的值即可，不要自己发明数值。
	namespace cursor_prio
	{
		constexpr int kArrow = 0;    // 兜底箭头（没有控件表态时）
		constexpr int kHand = 40;    // 可点控件：按钮、单选、链接
		constexpr int kText = 60;    // 文本编辑：I 形
		constexpr int kResize = 80;  // 尺寸调整：分隔条、滚动条车道
		constexpr int kDrag = 100;   // 拖拽 / 手势进行中，压过一切
	}
}
