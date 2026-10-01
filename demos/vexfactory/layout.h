#pragma once
#include <algorithm>

namespace vexfactory
{
struct Box { float x, y, width, height; };
struct Layout
{
    float width, height, scale;
    Box header, world, panel, panelTabs, panelBody, footer;
    static Layout make( float width, float height, float textScale )
    {
        Layout l{}; l.width = width; l.height = height; l.scale = std::clamp( textScale, 1.0f, 1.4f );
        const float pad = 16, head = 112 * l.scale, foot = 64 * l.scale;
        const float sidebar = std::min( 360 * l.scale, width * 0.48f );
        l.header = { 0, 0, width, head };
        l.footer = { 0, height - foot, width, foot };
        l.world = { pad, head + 8, width - sidebar - pad * 3, height - head - foot - 16 };
        l.panel = { width - sidebar - pad, head + 8, sidebar, l.world.height };
        l.panelTabs = { l.panel.x + 8, l.panel.y + 8, sidebar - 16, 42 * l.scale };
        l.panelBody = { l.panel.x + 12, l.panel.y + 58 * l.scale, sidebar - 24, l.panel.height - 66 * l.scale };
        return l;
    }
    // Font size depends only on the accessibility setting, never on world zoom
    // or window/framebuffer dimensions. Drawing uses logical screen coordinates.
    float bodyText() const { return 20 * scale; }
    float smallText() const { return 18 * scale; }
};
}
