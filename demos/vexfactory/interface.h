#pragma once
#include "layout.h"
#include "raylib.h"
#include <cmath>
#include <string>
#include <vector>

namespace vexfactory
{
inline const Color Background = { 17, 23, 30, 255 }, Panel = { 29, 38, 48, 255 }, Border = { 61, 77, 90, 255 };
inline const Color Ink = { 240, 238, 225, 255 }, Muted = { 177, 190, 201, 255 }, Mint = { 105, 222, 180, 255 }, Gold = { 252, 196, 93, 255 }, Coral = { 255, 143, 124, 255 };
inline const Color ProductColors[] = { Muted, { 146, 212, 248, 255 }, Gold, Mint };
inline Rectangle rect( Box b ) { return { b.x, b.y, b.width, b.height }; }
inline constexpr int ToolTiles[] = { 26, 96, 75, 87, 99, 123, 121, 58, 111, 90 };
inline constexpr int ItemTiles[] = { 74, 86, 114, 90 };
inline void sprite( Texture2D atlas, int tile, Rectangle dest, float rotation = 0, Color tint = WHITE )
{
    Rectangle src = { static_cast<float>( tile % 12 * 16 ), static_cast<float>( tile / 12 * 16 ), 16, 16 };
    DrawTexturePro( atlas, src, { dest.x + dest.width / 2, dest.y + dest.height / 2, dest.width, dest.height }, { dest.width / 2, dest.height / 2 }, rotation, tint );
}
inline void arrow( Vector2 center, int direction, Color color, float size )
{
    constexpr int dx[] = { 1, 0, -1, 0 }, dy[] = { 0, 1, 0, -1 };
    Vector2 d{ static_cast<float>( dx[direction] ), static_cast<float>( dy[direction] ) }, side{ -d.y, d.x };
    Vector2 tip{ center.x + d.x * size, center.y + d.y * size };
    DrawLineEx( { center.x - d.x * size + side.x * size, center.y - d.y * size + side.y * size }, tip, size * .3f, color );
    DrawLineEx( { center.x - d.x * size - side.x * size, center.y - d.y * size - side.y * size }, tip, size * .3f, color );
}
class FontFace
{
public:
    Font font{}; bool owned = false; float density = 0; std::string source;
    void choose( const std::string& overridePath )
    {
        if ( !overridePath.empty() && FileExists( overridePath.c_str() ) ) source = overridePath;
        if ( source.empty() ) for ( const char* path : {
#ifdef _WIN32
            "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
#elif defined( __APPLE__ )
            "/System/Library/Fonts/Supplemental/Arial.ttf", "/Library/Fonts/Arial.ttf", "/System/Library/Fonts/Supplemental/Verdana.ttf",
#else
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
#endif
            "" } ) if ( *path && FileExists( path ) ) { source = path; break; }
        update();
    }
    void update()
    {
        const float dpi = std::max( 1.0f, static_cast<float>( GetRenderWidth() ) / std::max( 1, GetScreenWidth() ) );
        if ( font.texture.id && std::abs( density - dpi ) < .05f ) return;
        if ( owned ) UnloadFont( font ); owned = false; density = dpi;
        if ( !source.empty() )
        {
            int points[95]; for ( int i = 0; i < 95; ++i ) points[i] = i + 32;
            font = LoadFontEx( source.c_str(), static_cast<int>( 56 * dpi ), points, 95 );
            owned = IsFontValid( font ) && font.texture.id != GetFontDefault().texture.id;
        }
        if ( !owned ) font = GetFontDefault();
        SetTextureFilter( font.texture, owned ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT );
    }
    void release() { if ( owned ) UnloadFont( font ); owned = false; }
};
struct Ui
{
    Font font; float scale; Vector2 mouse; bool input = true, clipped = false; Rectangle clip{};
    float body() const { return 20 * scale; }
    float small() const { return 18 * scale; }
    float measure( const std::string& text, float size ) const { return MeasureTextEx( font, text.c_str(), size, .3f ).x; }
    void box( Rectangle r, Color color = Panel ) const { DrawRectangleRec( r, color ); DrawRectangleLinesEx( r, 1, Border ); }
    bool hit( Rectangle r ) const { return input && CheckCollisionPointRec( mouse, r ) && ( !clipped || CheckCollisionPointRec( mouse, clip ) ); }
    void text( std::string text, float x, float y, float size, Color color = Ink, float width = 0 ) const
    {
        if ( width > 0 && measure( text, size ) > width )
        {
            while ( !text.empty() && measure( text + "...", size ) > width ) { int bytes = 1; GetCodepointPrevious( text.c_str() + text.size(), &bytes ); text.resize( text.size() - std::min( text.size(), static_cast<size_t>( std::max( 1, bytes ) ) ) ); }
            text += "...";
        }
        DrawTextEx( font, text.c_str(), { x, y }, size, .3f, color );
    }
    float wrap( const std::string& source, float x, float y, float width, float size, Color color = Ink, bool draw = true ) const
    {
        std::string line, word; float offset = 0; const float leading = size * 1.35f;
        const auto flush = [&]() { if ( draw ) text( line, x, y + offset, size, color ); line.clear(); offset += leading; };
        const auto addWord = [&]()
        {
            if ( word.empty() ) return;
            if ( !line.empty() && measure( line + " " + word, size ) > width ) flush();
            while ( measure( word, size ) > width && !word.empty() )
            {
                size_t n = 0;
                do { int bytes = 1; GetCodepointNext( word.c_str() + n, &bytes ); const size_t next = n + std::max( 1, bytes ); if ( next > word.size() || ( n > 0 && measure( word.substr( 0, next ), size ) > width ) ) break; n = next; } while ( n < word.size() );
                n = std::max<size_t>( 1, n ); line = word.substr( 0, n ); word.erase( 0, n ); flush();
            }
            if ( !line.empty() ) line += " "; line += word; word.clear();
        };
        for ( char ch : source )
        {
            if ( ch == ' ' || ch == '\n' ) { addWord(); if ( ch == '\n' ) flush(); }
            else word += ch;
        }
        addWord(); if ( !line.empty() ) flush(); return offset;
    }
    bool button( Rectangle r, const std::string& title, bool active = false, bool enabled = true ) const
    {
        const bool hover = enabled && hit( r ); box( r, active ? Color{ 39, 74, 67, 255 } : hover ? Color{ 50, 66, 80, 255 } : Panel );
        if ( active ) DrawRectangle( static_cast<int>( r.x ), static_cast<int>( r.y ), 3, static_cast<int>( r.height ), Mint );
        const float size = small(), w = std::min( measure( title, size ), r.width - 20 );
        text( title, r.x + ( r.width - w ) / 2, r.y + ( r.height - size ) / 2, size, enabled ? ( active ? Mint : Ink ) : Muted, r.width - 20 );
        return hover && IsMouseButtonPressed( MOUSE_BUTTON_LEFT );
    }
    void beginClip( Rectangle r ) { clipped = true; clip = r; BeginScissorMode( static_cast<int>( r.x ), static_cast<int>( r.y ), static_cast<int>( r.width ), static_cast<int>( r.height ) ); }
    void endClip() { EndScissorMode(); clipped = false; }
};
class Audio
{
    bool ready = false; Sound sounds[3]{};
public:
    void initialize( bool disabled )
    {
        if ( disabled ) return; InitAudioDevice(); ready = IsAudioDeviceReady(); if ( !ready ) return;
        for ( int effect = 0; effect < 3; ++effect )
        {
            const int frames = effect == 1 ? 16536 : 2205; std::vector<short> samples( frames );
            for ( int i = 0; i < frames; ++i )
            {
                const float progress = static_cast<float>( i ) / frames;
                const float frequency = effect == 0 ? 620 : effect == 2 ? 220 : ( progress < .33f ? 523.25f : progress < .66f ? 659.25f : 783.99f );
                const float envelope = std::min( 1.0f, i / 100.0f ) * ( 1 - progress );
                samples[i] = static_cast<short>( 3000 * envelope * std::sin( i * frequency * 6.2831853f / 22050 ) );
            }
            Wave wave{}; wave.frameCount = frames; wave.sampleRate = 22050; wave.sampleSize = 16; wave.channels = 1; wave.data = samples.data(); sounds[effect] = LoadSoundFromWave( wave );
        }
    }
    void play( int effect, bool muted ) { if ( ready && !muted ) PlaySound( sounds[effect] ); }
    void release() { if ( ready ) { for ( auto sound : sounds ) UnloadSound( sound ); CloseAudioDevice(); } ready = false; }
};
}
