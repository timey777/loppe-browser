/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <string>
#include "graphics_types.h"
#include <vector>
#include <memory>
#include "text_layout.h"
#include <cstdlib>
#include <cstring>
#include <unordered_set>
#include "loppe_export.h"
#include "components/uitypes.h"
#include "misc/render_stats.h"

class ResourceManager;

namespace loppe
{
    enum class DrawCommandType
    {
        Rectangle,
        Text,
        TextLayout,
        Image,
        SvgIcon,        // source = 注册名，fillColor = 着色
        Line,
        Circle,
        BoxShadowGauss, // bounds = 阴影形状盒，fillColor = 阴影色，cornerRadius = 形状圆角，
                        // gaussSigma = blur / 2；不支持的后端可忽略
        PushClip,
        PopClip,
        PushTransform,
        PopTransform,
        SubList
    };

    struct LOPPE_API DrawCommand
    {
        DrawCommandType type = DrawCommandType::Rectangle;

        ui::Rect bounds;
        ui::Color fillColor;
        ui::Color strokeColor;
        ui::Edges strokeWidths;

        // Rectangle
        float cornerRadius = 0.0f;

        // BoxShadowGauss
        float gaussSigma = 0.0f;

        // Text
        std::string text;
        std::string fontFamily;
        float fontSize = 0.0f;
        bool bold = false;
        bool italic = false;
        render::TextAlign textAlign = render::TextAlign::Left;
        float lineHeight = 0.0f;
        bool wrap = true;

        // TextLayout
        std::shared_ptr<ITextLayout> layout;
        // 可见行区间：textLineCount > 0 时只绘制 [textFirstLine, +textLineCount)，否则全量绘制
        int textFirstLine = 0;
        int textLineCount = 0;

        // Image / SvgIcon（Image: 图片路径；SvgIcon: 注册名）
        std::string source;

        // Transform
        float dx = 0.0f;
        float dy = 0.0f;

        // SubList
        std::shared_ptr<class DisplayList> layer;
    };

    class LOPPE_API DisplayList
    {
    public:
        DisplayList();

        void clear();

        void reserve(size_t n);

        // 去掉空操作命令（如空 clip / transform 作用域），不改变绘制结果
        void compact();

        void addRect(const ui::Rect &bounds, const ui::Color &fill, const ui::Color &stroke = ui::Color(),
                     const ui::Edges& strokeWidths = ui::Edges(), float cornerRadius = 0.0f);

        // 外阴影：shapeBounds = 阴影形状盒（已含 offset 与 spread）
        void addBoxShadowGauss(const ui::Rect &shapeBounds, const ui::Color &color,
                               float cornerRadius, float sigma);

        void addText(const std::string &text,
                     const ui::Rect &bounds,
                     const ui::Color &color,
                     float fontSize = 0.0f,
                     const std::string &fontFamily = "",
                     const TextLayoutParams &layout = TextLayoutParams());

        void addTextLayout(const std::shared_ptr<ITextLayout> &layout, const ui::Rect &bounds, const ui::Color &color,
                           int firstLine = 0, int lineCount = 0);

        void addImage(const std::string &source, const ui::Rect &bounds);

        // name = svg::RegisterIcon 注册名；tint 为着色（默认白 = 图标原色）
        void addSvgIcon(const std::string &name, const ui::Rect &bounds,
                        const ui::Color &tint = ui::Color(1.0f, 1.0f, 1.0f, 1.0f));

        void pushClip(const ui::Rect &bounds);
        void pushClip(const ui::Rect &bounds, float cornerRadius);

        void popClip();

        void pushTransform(float dx, float dy);

        void popTransform();

        void addLayer(std::shared_ptr<DisplayList> layer);

        // ---- 内部辅助 ----
        template<typename F>
        void emitCommand(F&& init) {
            DrawCommand cmd;
            init(cmd);
            commands_.push_back(std::move(cmd));
        }

        const std::vector<DrawCommand> &getCommands() const;

        size_t size() const;

        // 调试用文本序列化（逐行）；TextLayout 指针不序列化，会被跳过
        std::string serializeToText() const;
        static DisplayList deserializeFromText(const std::string &text);

        // 递归统计绘制命令到 `stats`（含 SubList 图层）
        void collectStats(RenderStats& stats) const;

    private:
        std::vector<DrawCommand> commands_;
    };

} // namespace loppe
