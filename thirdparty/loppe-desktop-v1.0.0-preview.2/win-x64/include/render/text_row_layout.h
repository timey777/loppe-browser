/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

// text_row_layout.h — 行粒度、按需物化、可增量改宽、可增量编辑的排版引擎。

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "render/graphics_types.h"
#include "render/text_layout.h"

namespace loppe {

// ---- 排版后端抽象 ----
class IRowLayoutBackend {
public:
	virtual ~IRowLayoutBackend() = default;

	// 码点水平 advance（px，已按 scale 归一）。要求不触发栅格化（纯度量）。
	// 返回值按「笔位 = 行首」的第一格宽度给出；调用方按真实笔位换算。
	virtual float glyphAdvance(uint32_t cp) = 0;

	// 确保字形已栅格化并回填栅格度量。返回 false 表示无字形。
	// 有字形但 tex==0（如过大被丢弃）时返回 true 且 out.tex==0（调用方不产生 quad）。
	struct GlyphData {
		float width = 0, height = 0;
		float bearingX = 0, bearingY = 0;
		float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
		unsigned tex = 0;
	};
	virtual bool ensureGlyph(uint32_t cp, GlyphData& out) = 0;
};

// ---- 行粒度懒布局引擎 ----
class TextRowLayout : public ITextLayout {
public:
	struct Params {
		bool wrap = false;                    // 是否按 maxWidth 折行
		float maxWidth = -1.0f;               // 折行宽度（wrap 时有效）
		render::TextAlign align = render::TextAlign::Left;
		float lineHeight = 16.0f;             // 每显示行高
		float ascent = 12.0f;                 // 基线到行顶距离（字形 y 定位用）
		float fontSize = 0.0f;                // 字号（制表位间距 = fontSize × kTabWidthEm）
	};

	// 字形 quad（行本地坐标：x/y 相对该行左上角，y 以行顶为 0）。
	struct GlyphQuad {
		float x = 0, y = 0;
		float w = 0, h = 0;
		float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
		unsigned tex = 0;
	};

	// 一个显示行（display row）对外视图。纯 POD；行号 = 全局显示顺序。
	struct Row {
		int startCP = 0;    // 全局 cp 下标（内容行首；空行 = 其后 '\n' 所在 cp）
		int lenCP = 0;      // 行内内容 cp 数（不含行尾 '\n' 标记）
		float width = 0.0f; // 行内容宽（advance 和，不含对齐偏移）
	};

	// 一个显示行的物化缓存（仅"被请求过"的行才非空）。
	struct RowMat {
		bool materialized = false;
		// 行物化重试次数上限；达到上限后不再重排该行。
		static constexpr int kMaxRetry = 8;
		int retry = 0;
		std::vector<render::Rect> rects; // lenCP 个字符矩形（行本地 x，含对齐偏移）
		std::vector<GlyphQuad> glyphs;   // 行本地坐标字形 quad（仅 tex!=0 的）
	};

	// text/cps/adv 需一一对应（由调用方解码并提供）。
	TextRowLayout(std::string text, std::vector<uint32_t> cps, std::vector<float> adv,
	              Params p, std::shared_ptr<IRowLayoutBackend> backend);

	// ITextLayout
	render::Rect getBounds() const override;
	std::string getText() const override;
	int getLineCount() const override;
	int getCharIndexAt(float x, float y) const override;
	render::Rect getCursorRect(int index) const override;
	std::vector<render::Rect> getSelectionRects(int startIndex, int endIndex) const override;

	// 增量改宽：同文本/同字体/同参数、仅 maxWidth 变化时调用（Left + wrap 有效）。
	// 返回 true 表示已处理（调用方无需重建布局）；false 表示不支持增量（调用方应重建布局）。
	bool updateMaxWidth(float newWidth) override;

	// 增量内容编辑：把 [startCp, startCp+delLenCp) 的文本替换为 insertedUtf8。
	// 调用方负责先把文本编辑落定；startCp/delLenCp 为整篇全局 cp（含 '\n'）。
	// 返回 true 表示已增量处理（调用方无需重建）；false 表示不支持（调用方应全量重建）。
	bool applyEdit(int startCp, int delLenCp, const std::string& insertedUtf8) override;

	// ---- 供渲染/测试 ----
	int cpCount() const;
	int rowCount() const;
	const Params& params() const;

	// 物化指定显示行（幂等；只为"被请求的行"生成几何）。
	void materializeLine(int lineIndex) const;
	// 全量物化（测试/调试用）。
	void materializeAll() const;

	// 显示行数据（按值返回）。
	Row rowAt(int lineIndex) const;
	// 行物化缓存指针（未物化返回 nullptr；渲染/测试在 materializeLine 后读取）。
	const RowMat* rowMatPtr(int lineIndex) const;
	// 某显示行首全局 y（等距行高）。
	float rowTop(int lineIndex) const;

	// 渲染辅助：把第 line 行的字形按全局坐标逐个交给 sink（含行内对齐偏移）。
	// lineY 传绘制原点 y（此时行 top 由内部累加）。
	void emitLineGlyphs(int lineIndex, float originX, float originY,
	                    const std::function<void(const GlyphQuad&, float gx, float gy)>& sink) const;

	// 测试辅助：每个 cp 的字符矩形全量视图（x 为行内相对坐标，y 为行顶全局坐标）。
	// 顺序按 cp 0..ncp-1。
	std::vector<render::Rect> allCharRectsForTest() const;

private:
	// ---- 参数与度量后端 ----
	Params params_;
	std::shared_ptr<IRowLayoutBackend> backend_; // 物化期字形度量（生命周期随 layout）

	// ---- 逻辑行（每行自持内容 + 行内相对 cp）----
	struct Seg {
		int startCP = 0;              // 本行内容首 cp 的全局前缀（编辑后重算）
		int lenCP = 0;                // 内容码点数（== cps.size()）
		float width = 0.0f;           // 内容总宽（advance 和；'\t' 按行首制表位算，仅作参考）
		int id = -1;                  // 内容模板 id
		std::vector<uint32_t> cps;    // 行内容码点（不含 '\n'）
		std::vector<float> adv;       // 与 cps 对齐的每码点 base advance（'\t' 存"从行首起的一格宽"）
		bool hasTab = false;          // 本行内容里是否有 '\t'（只作标记；宽度一律由 rowSpanWidth 结算）
	};
	std::vector<Seg> segs_;
	// 内容 → id 映射
	std::unordered_map<std::string, int> contentIds_;

	// 显示行内 [off, off+len) 的总宽；'\t' 按显示行行首（pen=0）结算。widthOnly=false 时把每码点实际 advance 写进 outAdv。
	float rowSpanWidth(const Seg& s, int off, int len, std::vector<float>* outAdv = nullptr) const;
	// 同上，但只算单个码点在给定行内 pen 处的实际 advance（materializeLine 用）。
	float rowCpAdvance(const Seg& s, int off, int k, float pen) const;

	// ---- 显示行（全局稠密；行内容引用"逻辑行 + 行内偏移"）----
	struct DRow {
		int seg = 0;    // 逻辑行下标（segs_）
		int off = 0;    // 内容在行内的相对 cp 偏移（显示行首）
		int lenCP = 0;  // 该显示行内容 cp 数
		float width = 0.0f;
	};
	mutable std::vector<DRow> rows_;                       // 全局显示行（顺序即行号）
	mutable std::vector<std::unique_ptr<RowMat>> mats_;    // 与 rows_ 平行：行物化缓存（懒分配）
	int totalCp_ = 0;                       // 全文码点数（含各逻辑行之间的 '\n'）
	float m_maxLineW_ = 0.0f;               // 全行最大内容宽（getBounds().w 缓存）

	// 由整篇 cps/adv 拆分逻辑行（构造时一次）。
	void buildSegmentsFromFull(const std::vector<uint32_t>& cps, const std::vector<float>& adv);
	// 按内容填一行的 cps/adv（原样拷贝；'\t' 由 rowSpanWidth 在成行时按行首结算）。
	void fillSegFromContent(Seg& seg, const uint32_t* cps, const float* adv, int len, int firstCpInSource);
	// 按本行行首（pen=0）结算 seg.width（只更新宽度缓存，不动 adv）。
	void syncTabWidths(Seg& seg);
	// 给一段行内容分配内容模板 id（相同内容共享；len 空/超长返回 -1）。
	int contentIdFor(const std::vector<uint32_t>& cps);
	// 由当前逻辑行折行填入显示行
	void fillRows(std::vector<DRow>& out) const;
	// 按当前参数重建显示行
	void buildRows();
	// 行宽度 w 的对齐偏移（Center/Right 与宽度相关；行本地 x 起点）。
	float lineAlignOffsetWidth(float width) const;

	// 第 rowIdx 个显示行的行首全局 cp。
	int rowStartCpOf(int rowIdx) const;
	// 任意全局 cp 的内容码点（'\n' 边界等）。cp 越界返回 0。
	uint32_t cpAt(int idx) const;
	// 定位"全局 cp → 拥有其字符矩形的行下标"。
	int rowOwningRectOfCP(int cp) const;
	// 任意 cp 的字符矩形（全局坐标：y=行顶）。cp 越界返回 {0,0,0,lh}。
	render::Rect rectOfCP(int cp) const;

	// 内容手术（applyEdit 内部）：替换 [startCp,startCp+delLenCp) 为 insCps/insAdv。
	void applyContentEdit(int startCp, int delLenCp,
	                      const std::vector<uint32_t>& insCps, const std::vector<float>& insAdv);
};

} // namespace loppe