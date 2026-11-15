/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "render/drawer.h"
#include "render/graphics_context.h"
#include "render/text_measurer.h"

namespace loppe {
class ITextRenderer;
}

class OpenGLTextRenderer;

class LOPPE_API OpenGLDrawerImpl : public IDrawer, public IGraphicsContext {
public:
	OpenGLDrawerImpl();
	~OpenGLDrawerImpl() override;

	// IGraphicsContext implementation
	void init(int width, int height) override;
	void resize(const loppe::FramebufferSizePx& framebufferSize,
		const loppe::ContentScale& contentScale = {}) override;
	void clear(float r, float g, float b, float a) override;
	void present() override;
	void shutdown() override;
	IDrawer* getDrawer() override;
	loppe::ITextRenderer* getTextRenderer() override;

	// IDrawer implementation
	void drawRect(float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) override;
	void drawRoundedRect(float x, float y, float w, float h, float rx, float ry, const DrawStyle& style = DrawStyle()) override;
	void drawGaussianShadow(float x, float y, float w, float h, float cornerRadius,
	                        float sigma, const DrawStyle& style = DrawStyle()) override;
	void drawImage(ImageHandle img, float x, float y, float w, float h, const DrawStyle& style = DrawStyle()) override;

	// ---- 字形提交 / 后端状态访问 ----
	// 把一个字形矩形送入共享纹理批
	void submitTextGlyph(unsigned int tex, float x, float y, float w, float h,
		float u0, float v0, float u1, float v1, const float color[4]);
	float getContentScaleX();
	float getOffsetX();
	float getOffsetY();

	ImageHandle createImageFromMemory(const unsigned char* data, int width, int height, int channels) override;
	void unloadImage(ImageHandle img) override;

	void setClipRect(const render::Rect& r) override;
	bool intersectClipRect(const render::Rect& r, render::Rect* outNewClip) override;
	bool getClipRect(render::Rect* outCurrent) const override;
	void resetClip() override;

	void setClipCornerRadius(float radius) override;
	float getClipCornerRadius() const override;

	void pushTransform(float dx, float dy) override;
	void popTransform() override;

public:
	class ImplData;

private:
	ImplData* d;
	OpenGLTextRenderer* textRenderer_ = nullptr;
};