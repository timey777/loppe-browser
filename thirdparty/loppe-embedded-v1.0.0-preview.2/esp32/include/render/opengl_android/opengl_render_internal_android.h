/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "loppe_export.h"
#include "opengl_drawer_android.h"

#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <ft2build.h>
#include FT_FREETYPE_H

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <chrono>

#ifndef LOPPE_RENDER_STATS
#define LOPPE_RENDER_STATS 0
#endif

struct Glyph {
	// Glyph texture comes from a font atlas page (not per-glyph texture)
	unsigned int tex = 0;
	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 1.0f;
	float v1 = 1.0f;
	float width = 0.0f;
	float height = 0.0f;
	float bearingX = 0.0f;
	float bearingY = 0.0f;
	float advance = 0.0f; // in DIP units (converted from 1/64 pixels)
};


class OpenGLDrawerAndroid::ImplData {
public:
	int width = 800, height = 600;
	float scaleX = 1.0f, scaleY = 1.0f;

	unsigned int progColor = 0;
	unsigned int progTex = 0;
	unsigned int progTexText = 0; // text shader: alpha from red channel
	unsigned int vao = 0, vbo = 0; // for textured quad (pos + uv)
	size_t vboFloatCapacity = 0; // capacity of vbo in floats
	unsigned int vaoColor = 0, vboColor = 0; // for rect batch (pos + rect + color + params)
	size_t vboColorFloatCapacity = 0; // capacity of vboColor in floats
	unsigned int whiteTex = 0;

	// uniform locations
	int uProj_color = -1;
	int uColor = -1;
	int uRect = -1;
	int uCornerRadius = -1;
	int uStrokeWidth = -1;
	int uSdfClip_color = -1;
	int uSdfClipRadius_color = -1;

	int uProj_tex = -1;
	int uTexSampler = -1;
	int uTexColor = -1;
	int uSdfClip_tex = -1;
	int uSdfClipRadius_tex = -1;

	int uProj_texText = -1;
	int uTexSamplerText = -1;
	int uTexColorText = -1;
	int uSdfClip_texText = -1;
	int uSdfClipRadius_texText = -1;

	// clip state (top-left origin, pixels)
	bool hasClip = false;
	render::Rect clip{ 0,0,0,0 };
	float clipCornerRadius = 0.0f; // SDF clip corner radius (0 = rectangular scissor only)

	// lightweight GL state cache (best-effort, scoped to this drawer)
	struct GLStateCache {
		unsigned int program = 0;
		unsigned int vao = 0;
		unsigned int arrayBuffer = 0;
		unsigned int tex0 = 0;
		bool scissorEnabled = false;
		int scissorX = -1;
		int scissorY = -1;
		int scissorW = -1;
		int scissorH = -1;
	} gl;

	// transform state
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	std::vector<std::pair<float, float>> transformStack;

	// FreeType
	FT_Library ft = nullptr;

	// simple per-frame render statistics
	bool stat_enabled = (LOPPE_RENDER_STATS != 0);
	int stat_drawCalls = 0;
	int stat_drawCalls_rect = 0;
	int stat_drawCalls_image = 0;
	int stat_drawCalls_text = 0;
	int stat_image_quads = 0;
	int stat_texBatch_flushes = 0;
	int stat_texBatch_flush_keyClip = 0;
	int stat_texBatch_flush_keyColor = 0;
	int stat_texBatch_flush_keyTex = 0;
	int stat_texBatch_flush_keyKind = 0;
	int stat_texBatch_flush_ordering = 0;
	int stat_texBatch_flush_clipChange = 0;
	int stat_texBatch_flush_present = 0;
	int stat_textureBinds = 0;
	int stat_bufferSubData = 0;
	int stat_bufferSubData_rect = 0;
	int stat_bufferSubData_image = 0;
	int stat_bufferSubData_text = 0;
	int stat_useProgram = 0;
	int stat_vaoBinds = 0;
	int stat_text_batches = 0;
	int stat_text_glyphs = 0;
	int stat_frames = 0;
	std::chrono::steady_clock::time_point stat_lastTime;
	struct FontKey {
		std::string family;
		int size;
		float scale;
		bool operator<(const FontKey& o) const;
	};
	std::map<FontKey, std::map<uint32_t, Glyph>> glyphCache;
	std::map<FontKey, FT_Face> faceCache;

	struct FontAtlasPage {
		unsigned int tex = 0;
		int width = 0;
		int height = 0;
		int penX = 0;
		int penY = 0;
		int rowH = 0;
	};
	std::map<FontKey, std::vector<FontAtlasPage>> atlasPages;

	enum class TexBatchKind : unsigned char {
		None = 0,
		Text = 1,
		Image = 2,
	};
	struct TexturedBatch {
		TexBatchKind kind = TexBatchKind::None;
		unsigned int tex = 0;
		float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool hasClip = false;
		render::Rect clip{ 0,0,0,0 };
		std::vector<float> verts; // pos(x,y) + uv(u,v) => 4 floats
		int textGlyphs = 0;
		int imageQuads = 0;
	} texBatch;

	struct RectBatch {
		bool active = false;
		bool hasClip = false;
		render::Rect clip{ 0,0,0,0 };
		std::vector<float> verts; // per-vertex: pos2 + rect4 + color4 + params2 => 12 floats
		int rects = 0;
	} rectBatch;

};

void gl_bind_tex0(OpenGLDrawerAndroid::ImplData* d, unsigned int tex);
// 绑定纹理到 0 号纹理单元，并让 GL 绑定状态跟踪（gl.tex0）与实际绑定保持一致。
void gl_force_bind_tex0(OpenGLDrawerAndroid::ImplData* d, unsigned int tex);
void rect_batch_flush(OpenGLDrawerAndroid::ImplData* d);
void textured_batch_append_quad(OpenGLDrawerAndroid::ImplData* d,
    OpenGLDrawerAndroid::ImplData::TexBatchKind kind,
    unsigned int tex,
    const float color[4],
    float x, float y, float w, float h,
    float u0, float v0, float u1, float v1);