#pragma once
#include <algorithm>
#include <cmath>

namespace vexfactory
{
inline constexpr float MinUiScale = 1, MaxUiScale = 2;
struct Box { float x, y, width, height; };
// GLFW uses points on Retina/Wayland, but physical pixels on Windows/X11.
// Keep rendering and input in the same logical space, including fullscreen.
struct Surface
{
    float width, height, windowWidth, windowHeight, framebufferWidth, framebufferHeight;
    static Surface make( float windowWidth, float windowHeight, float framebufferWidth, float framebufferHeight, float contentX, float contentY )
    {
        windowWidth = std::max( 1.0f, windowWidth ); windowHeight = std::max( 1.0f, windowHeight );
        framebufferWidth = std::max( 1.0f, framebufferWidth ); framebufferHeight = std::max( 1.0f, framebufferHeight );
        contentX = std::max( 1.0f, contentX ); contentY = std::max( 1.0f, contentY );
        const float width = framebufferWidth > windowWidth * 1.01f ? windowWidth : windowWidth / contentX;
        const float height = framebufferHeight > windowHeight * 1.01f ? windowHeight : windowHeight / contentY;
        return { width, height, windowWidth, windowHeight, framebufferWidth, framebufferHeight };
    }
    float densityX() const { return framebufferWidth / width; }
    float densityY() const { return framebufferHeight / height; }
    float mouseX( float native ) const { return native * width / windowWidth; }
    float mouseY( float native ) const { return native * height / windowHeight; }
    Box pixels( Box logical ) const { return { logical.x * densityX(), logical.y * densityY(), logical.width * densityX(), logical.height * densityY() }; }
};
struct Layout
{
    float width, height, scale;
    Box header, world, panel, panelTabs, panelBody, footer;
    static Layout make( float width, float height, float textScale, int cargoKinds = 4 )
    {
        Layout l{}; l.width = width; l.height = height; l.scale = std::clamp( textScale, MinUiScale, MaxUiScale );
        const float pad = 12, head = 8 + 68 * l.scale, foot = 64 + 28 * l.scale;
        // Large text can put orders on a third line. No persistent sidebar.
        const float orders = width < 1200 && ( l.scale > 1.4f || ( cargoKinds >= 3 && l.scale >= 1.25f ) ) ? 28 * l.scale : 0;
        l.header = { 0, 0, width, head + orders };
        l.footer = { 0, height - foot, width, foot };
        l.world = { pad, l.header.height + 8, width - pad * 2, height - l.header.height - foot - 16 };
        const float sidebar = std::min( 360 * l.scale, width - pad * 4 );
        l.panel = { width - sidebar - pad * 2, l.world.y + 8, sidebar, l.world.height - 16 };
        l.panelTabs = { l.panel.x + 12, l.panel.y + 8, sidebar - 24, 38 * l.scale };
        l.panelBody = { l.panel.x + 12, l.panel.y + 54 * l.scale, sidebar - 24, l.panel.height - 62 * l.scale };
        return l;
    }
    float bodyText() const { return 24 * scale; }
    float smallText() const { return 20 * scale; }
    float fitTiles( int columns, int rows ) const { return std::min( ( world.width - 16 ) / columns, ( world.height - 16 ) / rows ); }
    // Initial views never shrink the factory to unreadable dots. Home can fit all.
    float tileSize( int columns, int rows, bool fitAll = false ) const { return fitAll ? fitTiles( columns, rows ) : std::max( 40.0f, fitTiles( columns, rows ) ); }
};
}
