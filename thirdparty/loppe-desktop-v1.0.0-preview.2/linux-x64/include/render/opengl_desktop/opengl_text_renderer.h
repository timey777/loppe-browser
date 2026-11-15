/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

// OpenGLTextRenderer: the OpenGL backend implementation of loppe::ITextRenderer
// (text measurement / layout / glyph drawing).

#include "opengl_drawer_impl.h"
#include "render/text_renderer.h"

#include <memory>
#include <string>
#include <vector>

class OpenGLTextRenderer : public loppe::ITextRenderer {
public:
	using ImplData = OpenGLDrawerImpl::ImplData;

	explicit OpenGLTextRenderer(OpenGLDrawerImpl::ImplData* backend);

	// ITextMeasurer
	TextMetrics measureText(const std::u32string& text, const FontDesc& font, float wrapWidth = -1.0f) override;
	TextMetrics measureText(const std::string& utf8text, const FontDesc& font, float wrapWidth = -1.0f) override;
	float getLineHeight(const FontDesc& font) override;
	std::shared_ptr<ITextLayout> createTextLayout(const std::string& text, const Font& font,
		const TextLayoutParams& params, float maxWidth = -1.0f) override;

	// ITextRenderer
	void drawText(const std::string& text, float x, float y, const Font& font, const DrawStyle& style,
		float maxWidth, const TextLayoutParams& layout) override;
	void drawTextLayout(const std::shared_ptr<ITextLayout>& layout, float x, float y, const DrawStyle& style) override;
	void drawTextLayoutRange(const std::shared_ptr<ITextLayout>& layout, float x, float y, const DrawStyle& style,
		int firstLine, int lineCount) override;

private:
	// 上次解码/度量的缓存（命中条件：字体/字号/缩放/斜体/文本 均未变）。
	struct DecodeCache {
		bool valid = false;
		std::string family;
		int size = 0;
		float scale = 0.0f;
		bool italic = false;
		std::string text;
		std::u32string cps;
		std::vector<float> adv;
	};

	OpenGLDrawerImpl::ImplData* d;
	DecodeCache dec_;
};