/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include "render/display_list.h"
#include "misc/render_stats.h"
#include <vector>
#include <string>
#include <memory>

namespace loppe {

class Scene;
class Panel;
class Text;

/**
 * @brief Developer debug overlay — draws real-time render stats on screen.
 *
 * Draws real-time render stats on screen as an independent overlay
 * (does not participate in the main scene's layout or event handling).
 *
 * Usage:
 *   DebugOverlay overlay;
 *   overlay.render(displayList, stats, avgFrameUs, scene);
 *
 * Toggle with F3: overlay.toggle()
 */
class DebugOverlay {
public:
    DebugOverlay() = default;

    /**
     * @brief Render the stats overlay onto the DisplayList.
     * @param dl          Target DisplayList (appended at end → paints on top).
     * @param stats       Per-frame render statistics.
     * @param avgFrameUs  Average frame time in microseconds.
     * @param scene       The active scene (for text-measurer access).
     */
    void render(DisplayList& dl, const RenderStats& stats, double avgFrameUs,
                Scene* scene);

    void setVisible(bool v);
    bool isVisible() const;
    void toggle();

    /** Call from key callback: F3 toggles overlay. */
    bool onKey(int key, int action);

private:
    bool visible_ = true;

    // ── Styling constants ──
    static constexpr float kFontSize    = 13.0f;
    static constexpr float kLineHeight  = 17.0f;
    static constexpr float kPadX        = 8.0f;
    static constexpr float kPadY        = 6.0f;
    static constexpr float kSectionGap  = 4.0f;
    static constexpr float kOriginX     = 8.0f;
    static constexpr float kOriginY     = 8.0f;
    static constexpr float kLabelWidth  = 115.0f;  // "  Visited:" etc.
    static constexpr float kValueMinW   = 80.0f;   // minimum value column

    // ── Engine-native controls (created once, updated per-frame) ──
    std::shared_ptr<Panel> panel_;
    std::vector<std::shared_ptr<Text>> rows_;
    int lastTotalControls_ = -1;   // to detect when we need to rebuild
    bool built_ = false;

    void buildControls(Scene* scene);
    void updateContent(const RenderStats& stats, double avgFrameUs);
    float computePanelWidth(Scene* scene);
};

} // namespace loppe