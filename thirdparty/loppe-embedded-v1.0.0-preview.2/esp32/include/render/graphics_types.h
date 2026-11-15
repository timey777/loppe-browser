/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <string>
#include <cstdint>
#include <cctype>

using ImageHandle = uint32_t;
namespace render
{
	// 文本水平对齐方式
	enum class TextAlign : uint8_t {
		Left,
		Center,
		Right,
		Justify
	};

	// 将字符串对齐方式（"left"/"center"/"right"/"justify"，大小写不敏感）转为枚举
	TextAlign textAlignFromString(const std::string& s);

	// 将枚举对齐方式转为字符串（用于序列化等需要字符串的场景）
	std::string textAlignToString(TextAlign a);

	struct Color {
		float r = 0, g = 0, b = 0, a = 1;
	};

	struct Rect {
		float x = 0, y = 0, w = 0, h = 0;
	};

}
struct Font {
	std::string family;
	float size = 0.0f;
	bool bold = false;
	bool italic = false;
};

// 文本布局参数：对齐/是否换行/行高等
struct TextLayoutParams {
	render::TextAlign textAlign = render::TextAlign::Left; // 对齐方式（枚举）
	bool wrap = true; // 是否允许换行
	float lineHeight = 0.0f; // 行高（像素），<=0 表示使用默认
};

struct DrawStyle {
	render::Color fillColor;
	render::Color strokeColor;
	float opacity = 1.0f;
	float cornerRadius = 0.0f;

	struct {
		float top = 0.0f, right = 0.0f, bottom = 0.0f, left = 0.0f;
		
		bool any() const { return top > 0 || right > 0 || bottom > 0 || left > 0; }
		bool isUniform() const { return top == right && right == bottom && bottom == left; }
		float first() const { return top; }
	} strokeWidths;
};