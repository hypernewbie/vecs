#define NOMINMAX
#include "simulation.h"
#include "raylib.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef VEX_FACTORY_DEFAULT_ASSET_DIR
#define VEX_FACTORY_DEFAULT_ASSET_DIR "temp/vexfactory/assets"
#endif

namespace
{
using namespace vexfactory;
constexpr int CanvasWidth = 1440, CanvasHeight = 900;
constexpr float GridX = 32, GridY = 144, TileSize = 40;
constexpr float PanelX = 1020, PanelWidth = 388;
const Color Background = { 19, 24, 32, 255 };
const Color Panel = { 29, 36, 46, 255 };
const Color Border = { 51, 62, 76, 255 };
const Color Text = { 233, 229, 216, 255 };
const Color Muted = { 147, 160, 174, 255 };
const Color Mint = { 112, 220, 180, 255 };
const Color Gold = { 247, 185, 79, 255 };
const Color Coral = { 240, 119, 101, 255 };
const Color ProductColors[] = { Muted, { 133, 199, 230, 255 }, Gold, Mint };
constexpr int ToolTiles[] = { 26, 96, 75, 87, 99, 123, 121 };
constexpr int ItemTiles[] = { 74, 86, 114, 90 };
constexpr const char* ToolHints[] = {
    "Moves one item per tile. Rotate with R.", "Produces ore. Output needs a conveyor.",
    "Ore -> plate  /  1.1 seconds", "Plate -> gear  /  1.5 seconds",
    "Gear -> engine  /  2.0 seconds", "Accepts all items from any side.", "Deletes a tile and scraps its contents."
};

void label( const char* text, float x, float y, float size, Color color = Text )
{
    DrawTextEx( GetFontDefault(), text, { x, y }, size, 1, color );
}
void card( Rectangle rect, Color fill = Panel )
{
    DrawRectangleRec( rect, fill );
    DrawRectangleLinesEx( rect, 1, Border );
}
Rectangle toolRect( int i ) { return { PanelX + 12, 184.0f + i * 43.0f, PanelWidth - 24, 38 }; }
Rectangle pauseRect() { return { PanelX, 806, 120, 36 }; }
Rectangle speedRect() { return { PanelX + 130, 806, 120, 36 }; }
Rectangle resetRect() { return { PanelX + 260, 806, 128, 36 }; }
Rectangle emptyRect() { return { PanelX + 260, 850, 128, 28 }; }
bool hit( Rectangle rect, Vector2 mouse ) { return CheckCollisionPointRec( mouse, rect ); }

void sprite( Texture2D atlas, int tile, Rectangle dest, float rotation = 0, Color tint = WHITE )
{
    const Rectangle src = { static_cast<float>( tile % 12 * 16 ), static_cast<float>( tile / 12 * 16 ), 16, 16 };
    DrawTexturePro( atlas, src, { dest.x + dest.width / 2, dest.y + dest.height / 2, dest.width, dest.height },
        { dest.width / 2, dest.height / 2 }, rotation, tint );
}
void arrow( Vector2 center, int direction, Color color, float size = 5 )
{
    const Vector2 d = { static_cast<float>( DX[direction] ), static_cast<float>( DY[direction] ) };
    const Vector2 side = { -d.y, d.x };
    const Vector2 tip = { center.x + d.x * size, center.y + d.y * size };
    const Vector2 a = { center.x - d.x * size + side.x * size, center.y - d.y * size + side.y * size };
    const Vector2 b = { center.x - d.x * size - side.x * size, center.y - d.y * size - side.y * size };
    DrawLineEx( a, tip, 2, color ); DrawLineEx( b, tip, 2, color );
}
void button( Rectangle rect, const char* text, Vector2 mouse, bool active = false )
{
    const bool hover = hit( rect, mouse );
    card( rect, active ? Color{ 37, 68, 64, 255 } : hover ? Color{ 48, 59, 72, 255 } : Panel );
    float fontSize = 18;
    while ( fontSize > 12 && MeasureTextEx( GetFontDefault(), text, fontSize, 1 ).x > rect.width - 16 ) fontSize -= 1;
    const float width = MeasureTextEx( GetFontDefault(), text, fontSize, 1 ).x;
    label( text, rect.x + ( rect.width - width ) / 2, rect.y + ( rect.height - fontSize ) / 2, fontSize, active ? Mint : Text );
}

struct View
{
    Tool tool = Tool::Belt;
    int direction = 0;
    bool paused = false;
    int speed = 1;
    int pendingReset = -1;
    float accumulator = 0;
    double milliseconds = 0;
    int lastPaintX = -1, lastPaintY = -1;
};

Vector2 logicalMouse( Rectangle viewport )
{
    Vector2 p = GetMousePosition();
    if ( !hit( viewport, p ) ) return { -100, -100 };
    return { ( p.x - viewport.x ) * CanvasWidth / viewport.width,
        ( p.y - viewport.y ) * CanvasHeight / viewport.height };
}

void updateInput( Simulation& sim, View& view, Vector2 mouse )
{
    if ( view.pendingReset >= 0 )
    {
        if ( IsKeyPressed( KEY_ESCAPE ) || ( IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) && hit( { 735, 485, 160, 44 }, mouse ) ) )
            view.pendingReset = -1;
        else if ( IsKeyPressed( KEY_ENTER ) || ( IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) && hit( { 545, 485, 160, 44 }, mouse ) ) )
        {
            sim.reset( view.pendingReset == 1 );
            view.pendingReset = -1;
            view.accumulator = 0;
            view.paused = false;
            view.lastPaintX = view.lastPaintY = -1;
        }
        return;
    }
    for ( int i = 0; i < 6; ++i ) if ( IsKeyPressed( KEY_ONE + i ) ) view.tool = static_cast<Tool>( i );
    if ( IsKeyPressed( KEY_ZERO ) ) view.tool = Tool::Erase;
    if ( IsKeyPressed( KEY_R ) ) view.direction = ( view.direction + 1 ) % 4;
    if ( IsKeyPressed( KEY_SPACE ) ) view.paused = !view.paused;
    if ( IsKeyPressed( KEY_TAB ) ) view.speed = view.speed == 4 ? 1 : view.speed * 2;
    if ( IsKeyPressed( KEY_L ) ) view.pendingReset = 1;
    if ( IsKeyPressed( KEY_N ) ) view.pendingReset = 0;
    if ( IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) )
    {
        for ( int i = 0; i < 7; ++i ) if ( hit( toolRect( i ), mouse ) ) view.tool = static_cast<Tool>( i );
        if ( hit( pauseRect(), mouse ) ) view.paused = !view.paused;
        if ( hit( speedRect(), mouse ) ) view.speed = view.speed == 4 ? 1 : view.speed * 2;
        if ( hit( resetRect(), mouse ) ) view.pendingReset = 1;
        if ( hit( emptyRect(), mouse ) ) view.pendingReset = 0;
    }
    if ( view.pendingReset >= 0 ) return;
    const int x = static_cast<int>( std::floor( ( mouse.x - GridX ) / TileSize ) );
    const int y = static_cast<int>( std::floor( ( mouse.y - GridY ) / TileSize ) );
    if ( !Simulation::inside( x, y ) ) { view.lastPaintX = view.lastPaintY = -1; return; }
    if ( IsKeyPressed( KEY_E ) )
    {
        const vecsEntity e = sim.at( x, y );
        if ( e != VECS_INVALID_ENTITY )
        {
            view.tool = vecsGet<Building>( sim.world(), e )->tool;
            view.direction = vecsGet<Cell>( sim.world(), e )->direction;
        }
    }
    // A single click places a machine. Belts and the eraser can paint while held.
    const bool paint = IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) ||
        ( ( view.tool == Tool::Belt || view.tool == Tool::Erase ) && IsMouseButtonDown( MOUSE_BUTTON_LEFT ) );
    const bool erase = IsMouseButtonDown( MOUSE_BUTTON_RIGHT );
    if ( ( paint || erase ) && ( x != view.lastPaintX || y != view.lastPaintY ||
        IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) || IsMouseButtonPressed( MOUSE_BUTTON_RIGHT ) ) )
    {
        sim.place( x, y, erase ? Tool::Erase : view.tool, view.direction );
        view.lastPaintX = x; view.lastPaintY = y;
    }
    if ( !IsMouseButtonDown( MOUSE_BUTTON_LEFT ) && !erase ) view.lastPaintX = view.lastPaintY = -1;
}

void drawGrid( Simulation& sim, const View& view, Texture2D atlas, Vector2 mouse )
{
    card( { GridX - 6, GridY - 6, Width * TileSize + 12, Height * TileSize + 12 }, { 48, 46, 52, 255 } );
    for ( int y = 0; y < Height; ++y ) for ( int x = 0; x < Width; ++x )
    {
        const Rectangle tile = { GridX + x * TileSize, GridY + y * TileSize, TileSize, TileSize };
        sprite( atlas, ( x * 7 + y * 11 ) % 13 == 0 ? 1 : 0, tile, 0, { 159, 146, 139, 255 } );
        DrawRectangleLinesEx( tile, 1, { 57, 52, 54, 30 } );
    }
    // Lane labels occupy empty floor, not production cells.
    constexpr const char* lanes[] = { "01  /  PLATES", "02  /  GEARS", "03  /  ENGINES" };
    for ( int i = 0; i < 3; ++i ) label( lanes[i], GridX + 14, GridY + ( i * 5 + 1 ) * TileSize + 8, 16, { 68, 53, 47, 255 } );
    sim.buildings( [&]( vecsEntity e, Cell& cell, Building& building )
    {
        const Rectangle tile = { GridX + cell.x * TileSize, GridY + cell.y * TileSize, TileSize, TileSize };
        if ( building.tool == Tool::Belt )
        {
            sprite( atlas, 26, tile, cell.direction * 90.0f );
            const float phase = static_cast<float>( std::fmod( sim.stats().elapsed * 3.0, 1.0 ) );
            const float offset = ( phase - 0.5f ) * 24;
            DrawCircleV( { tile.x + 20 + DX[cell.direction] * offset, tile.y + 20 + DY[cell.direction] * offset }, 1.5f, { 213, 223, 226, 120 } );
            return;
        }
        DrawRectangle( static_cast<int>( tile.x + 2 ), static_cast<int>( tile.y + 2 ), 36, 36, { 53, 59, 68, 255 } );
        sprite( atlas, ToolTiles[static_cast<int>( building.tool )], { tile.x + 2, tile.y + 2, 36, 36 } );
        if ( building.tool != Tool::Shipping )
        {
            const Vector2 outlet = { tile.x + 20 + DX[cell.direction] * 14.0f, tile.y + 20 + DY[cell.direction] * 14.0f };
            DrawCircleV( outlet, 7, Background );
            arrow( outlet, cell.direction, Mint, 3 );
        }
        if ( const Processor* proc = vecsGet<Processor>( sim.world(), e ) )
        {
            const Color status = proc->ready ? Coral : proc->working ? Mint : Muted;
            DrawRectangle( static_cast<int>( tile.x + 4 ), static_cast<int>( tile.y + 34 ), 32, 4, Background );
            const float progress = proc->progress / durationFor( proc->recipe );
            DrawRectangle( static_cast<int>( tile.x + 4 ), static_cast<int>( tile.y + 34 ), static_cast<int>( 32 * progress ), 4, status );
            const uint32_t stored = vecsGet<Inventory>( sim.world(), e )->stored;
            for ( uint32_t n = 0; n < stored; ++n ) DrawRectangle( static_cast<int>( tile.x + 3 + n * 5 ), static_cast<int>( tile.y + 3 ), 3, 3, Gold );
            if ( proc->working )
            {
                const float pulse = 0.5f + 0.5f * std::sin( static_cast<float>( sim.stats().elapsed ) * 8 );
                DrawCircle( static_cast<int>( tile.x + 6 ), static_cast<int>( tile.y + 28 ), 2 + pulse, Mint );
            }
        }
        if ( building.tool == Tool::Shipping ) DrawCircle( static_cast<int>( tile.x + 7 ), static_cast<int>( tile.y + 7 ), 3, Mint );
    } );
    sim.parcels( [&]( vecsEntity, Parcel& parcel, Transit& t )
    {
        const float x = GridX + ( t.fromX + ( t.x - t.fromX ) * t.progress + 0.5f ) * TileSize;
        const float y = GridY + ( t.fromY + ( t.y - t.fromY ) * t.progress + 0.5f ) * TileSize;
        DrawEllipse( static_cast<int>( x ), static_cast<int>( y + 8 ), 11, 4, { 0, 0, 0, 80 } );
        sprite( atlas, ItemTiles[static_cast<int>( parcel.kind )], { x - 13, y - 13, 26, 26 } );
    } );
    const int hx = static_cast<int>( std::floor( ( mouse.x - GridX ) / TileSize ) );
    const int hy = static_cast<int>( std::floor( ( mouse.y - GridY ) / TileSize ) );
    if ( Simulation::inside( hx, hy ) && view.pendingReset < 0 )
    {
        const Rectangle tile = { GridX + hx * TileSize, GridY + hy * TileSize, TileSize, TileSize };
        const Color color = view.tool == Tool::Erase ? Coral : Mint;
        DrawRectangleRec( tile, Fade( color, 0.18f ) );
        DrawRectangleLinesEx( tile, 2, color );
        sprite( atlas, ToolTiles[static_cast<int>( view.tool )], tile,
            view.tool == Tool::Belt ? view.direction * 90.0f : 0, Fade( WHITE, 0.45f ) );
        if ( view.tool != Tool::Erase && view.tool != Tool::Shipping ) arrow( { tile.x + 20, tile.y + 20 }, view.direction, color );
    }
}

void drawUI( Simulation& sim, const View& view, Texture2D atlas, Vector2 mouse )
{
    const Stats& stats = sim.stats();
    sprite( atlas, 114, { 32, 28, 48, 48 }, static_cast<float>( stats.elapsed * 20 ) );
    label( "VexFactory", 96, 25, 40 );
    label( "A SMALL FACTORY. A LOT OF MOVING PARTS.", 98, 72, 16, Muted );
    card( { 537, 26, 260, 72 } );
    label( "SHIPPING / SIM MINUTE", 551, 36, 16, Muted );
    label( TextFormat( "%.1f", stats.perMinute ), 551, 58, 28, Mint );
    card( { 811, 26, 240, 72 } );
    label( "ITEMS / WAITING", 825, 36, 16, Muted );
    label( TextFormat( "%u / %u", stats.items, stats.waiting ), 825, 58, 28, Gold );
    card( { 1065, 26, 343, 72 } );
    label( "LIVE ECS ENTITIES", 1079, 36, 16, Muted );
    label( TextFormat( "%u", sim.entities() ), 1079, 58, 28 );
    label( TextFormat( "%.3f ms/tick", view.milliseconds ), 1197, 69, 16, Muted );
    DrawLine( 32, 118, 1408, 118, Border );
    label( "PRODUCTION FLOOR", 32, 119, 16, Muted );
    label( view.paused ? "PAUSED" : "LIVE", 873, 119, 16, view.paused ? Gold : Mint );
    label( TextFormat( "%dx", view.speed ), 949, 119, 16, Text );

    card( { PanelX, 144, PanelWidth, 357 } );
    label( "BUILD", PanelX + 16, 156, 18, Mint );
    label( "UNLIMITED PARTS", PanelX + 204, 158, 16, Muted );
    for ( int i = 0; i < 7; ++i )
    {
        const Rectangle rect = toolRect( i );
        const bool selected = static_cast<int>( view.tool ) == i;
        card( rect, selected ? Color{ 41, 66, 62, 255 } : hit( rect, mouse ) ? Color{ 43, 53, 66, 255 } : Background );
        if ( selected ) DrawRectangle( static_cast<int>( rect.x ), static_cast<int>( rect.y ), 3, static_cast<int>( rect.height ), Mint );
        label( i == 6 ? "0" : TextFormat( "%d", i + 1 ), rect.x + 9, rect.y + 10, 18, Muted );
        sprite( atlas, ToolTiles[i], { rect.x + 33, rect.y + 4, 30, 30 }, i == 0 ? view.direction * 90.0f : 0 );
        label( toolName( static_cast<Tool>( i ) ), rect.x + 74, rect.y + 10, 18, selected ? Mint : Text );
        if ( selected ) arrow( { rect.x + rect.width - 20, rect.y + 19 }, view.direction, Mint );
    }

    card( { PanelX, 513, PanelWidth, 125 } );
    constexpr const char* directions[] = { "EAST", "SOUTH", "WEST", "NORTH" };
    label( TextFormat( "OUTPUT  %s  /  R TO ROTATE", directions[view.direction] ), PanelX + 16, 527, 16, Gold );
    label( ToolHints[static_cast<int>( view.tool )], PanelX + 16, 553, 16, Text );
    const int x = static_cast<int>( std::floor( ( mouse.x - GridX ) / TileSize ) );
    const int y = static_cast<int>( std::floor( ( mouse.y - GridY ) / TileSize ) );
    const vecsEntity hovered = sim.at( x, y );
    if ( hovered != VECS_INVALID_ENTITY )
    {
        const Building& b = *vecsGet<Building>( sim.world(), hovered );
        label( TextFormat( "%02d,%02d  %s", x, y, toolName( b.tool ) ), PanelX + 16, 580, 16, Muted );
        if ( const Processor* p = vecsGet<Processor>( sim.world(), hovered ) )
            label( TextFormat( "BUFFER %u/4  /  %s", vecsGet<Inventory>( sim.world(), hovered )->stored,
                p->ready ? "OUTPUT BLOCKED" : p->working ? "PROCESSING" : "NEEDS INPUT" ), PanelX + 16, 605, 16, p->ready ? Coral : Mint );
        else label( "E picks this part and its direction.", PanelX + 16, 605, 16, Muted );
    }
    else
    {
        label( "Click to place. Drag to paint belts.", PanelX + 16, 580, 16, Muted );
        label( "RMB removes parts and scraps contents.", PanelX + 16, 605, 16, Muted );
    }

    card( { PanelX, 650, PanelWidth, 134 } );
    const bool fulfilled = stats.delivered[1] >= 20 && stats.delivered[2] >= 15 && stats.delivered[3] >= 10;
    label( fulfilled ? "CONTRACTS FULFILLED" : "SHIPPING CONTRACTS", PanelX + 16, 662, 18, fulfilled ? Mint : Text );
    constexpr uint32_t goals[] = { 20, 15, 10 };
    for ( int i = 0; i < 3; ++i )
    {
        const float y0 = 692.0f + i * 28;
        const uint32_t delivered = stats.delivered[i + 1];
        sprite( atlas, ItemTiles[i + 1], { PanelX + 16, y0, 22, 22 } );
        label( itemName( static_cast<Item>( i + 1 ) ), PanelX + 48, y0 + 3, 16, ProductColors[i + 1] );
        label( TextFormat( "%u / %u", delivered, goals[i] ), PanelX + 275, y0 + 3, 16, delivered >= goals[i] ? Mint : Muted );
        DrawRectangle( static_cast<int>( PanelX + 136 ), static_cast<int>( y0 + 8 ), 120, 6, Background );
        DrawRectangle( static_cast<int>( PanelX + 136 ), static_cast<int>( y0 + 8 ),
            static_cast<int>( 120 * std::min( 1.0f, static_cast<float>( delivered ) / goals[i] ) ), 6, ProductColors[i + 1] );
    }
    button( pauseRect(), view.paused ? "RESUME" : "PAUSE", mouse, view.paused );
    button( speedRect(), TextFormat( "SPEED %dx", view.speed ), mouse );
    button( resetRect(), "STARTER", mouse );
    button( emptyRect(), "EMPTY FLOOR", mouse );
    label( TextFormat( "SIM %.0fs  /  %u working", stats.elapsed, stats.machinesWorking ), PanelX, 854, 16, Muted );

    card( { 32, 806, 960, 72 } );
    for ( int i = 0; i < 4; ++i )
    {
        const float px = 46.0f + i * 237;
        sprite( atlas, ItemTiles[i], { px, 815, 26, 26 } );
        label( itemName( static_cast<Item>( i ) ), px + 34, 821, 18, ProductColors[i] );
        if ( i < 3 ) arrow( { px + 208, 828 }, 0, Muted );
    }
    label( "1-6 parts  /  R rotate  /  E pick  /  SPACE pause  /  TAB speed  /  L starter  /  N empty", 48, 853, 16, Muted );
    label( "VECS + RAYLIB  /  KENNEY TINY FACTORY (CC0)", 34, 884, 12, Muted );
    if ( view.pendingReset >= 0 )
    {
        DrawRectangle( 0, 0, CanvasWidth, CanvasHeight, { 0, 0, 0, 190 } );
        card( { 485, 348, 470, 213 } );
        label( view.pendingReset == 1 ? "RELOAD STARTER?" : "CLEAR THE FLOOR?", 517, 381, 28, Gold );
        label( "This resets the floor and shipping counters.", 517, 433, 18, Muted );
        button( { 545, 485, 160, 44 }, "RESET / ENTER", mouse, true );
        button( { 735, 485, 160, 44 }, "CANCEL / ESC", mouse );
    }
}
}

int main( int argc, char** argv )
{
    std::string assets = VEX_FACTORY_DEFAULT_ASSET_DIR;
    std::string screenshot;
    int frameLimit = 0;
    bool smoke = false;
    for ( int i = 1; i < argc; ++i )
    {
        if ( std::strcmp( argv[i], "--assets" ) == 0 && i + 1 < argc ) assets = argv[++i];
        else if ( std::strcmp( argv[i], "--screenshot" ) == 0 && i + 1 < argc ) { screenshot = argv[++i]; smoke = true; }
        else if ( std::strcmp( argv[i], "--smoke-test" ) == 0 ) smoke = true;
        else if ( std::strcmp( argv[i], "--frames" ) == 0 && i + 1 < argc )
        {
            char* end = nullptr;
            const long frames = std::strtol( argv[++i], &end, 10 );
            if ( *end || frames <= 0 || frames > 1000000 ) { std::fprintf( stderr, "--frames needs a number from 1 to 1000000.\n" ); return 1; }
            frameLimit = static_cast<int>( frames );
        }
        else if ( std::strcmp( argv[i], "--help" ) == 0 )
        {
            int major = 0, minor = 0, revision = 0;
            glfwGetVersion( &major, &minor, &revision );
            std::printf( "VexFactory / raylib %s / GLFW %d.%d.%d\n", RAYLIB_VERSION, major, minor, revision );
            std::puts( "VexFactory [--assets DIR] [--smoke-test] [--screenshot FILE.png] [--frames N]\n"
                "1-6: parts; 0: erase; R: rotate output; E: pick hovered part; space: pause; tab: speed.\n"
                "L: reload starter; N: empty floor; right mouse: erase; escape: close/cancel.\n"
                "Fetch artwork: cmake -P demos/vexfactory/fetch_assets.cmake" );
            return 0;
        }
        else { std::fprintf( stderr, "Unknown or incomplete argument: %s. Use --help.\n", argv[i] ); return 1; }
    }
    const std::string tilemap = assets + "/Tilemap/tilemap_packed.png";
    if ( !FileExists( tilemap.c_str() ) )
    {
        std::fprintf( stderr, "VexFactory assets are missing: %s\n"
            "Run: cmake -P demos/vexfactory/fetch_assets.cmake\n"
            "Or:  cmake --build <build-dir> --target vex_factory_assets\n"
            "Use --assets DIR for a different asset directory.\n", tilemap.c_str() );
        return 1;
    }
    Image image = LoadImage( tilemap.c_str() );
    if ( image.data == nullptr || image.width != 192 || image.height != 176 )
    {
        std::fprintf( stderr, "The tilemap must be Kenney Tiny Factory 1.0 (192 x 176). Fetch the pinned pack again.\n" );
        if ( image.data ) UnloadImage( image );
        return 1;
    }
    // Check for an available desktop before starting the graphics context.
    // Monitor-less hosts can still run the asset-free simulation tests.
    if ( !glfwInit() )
    {
        const char* error = nullptr;
        glfwGetError( &error );
        std::fprintf( stderr, "VexFactory needs a desktop display: %s\n", error ? error : "GLFW initialization failed" );
        UnloadImage( image );
        return 1;
    }
    int monitors = 0;
    glfwGetMonitors( &monitors );
    if ( monitors == 0 )
    {
        std::fprintf( stderr, "VexFactory found no desktop monitors. Use a desktop session, or Xvfb with software OpenGL.\n" );
        glfwTerminate();
        UnloadImage( image );
        return 1;
    }
    SetConfigFlags( FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT );
    InitWindow( CanvasWidth, CanvasHeight, "VexFactory | Vecs logistics sandbox" );
    if ( !IsWindowReady() ) { UnloadImage( image ); return 1; }
    SetWindowMinSize( 960, 600 );
    SetExitKey( KEY_NULL );
    SetTargetFPS( 60 );
    Texture2D atlas = LoadTextureFromImage( image );
    UnloadImage( image );
    SetTextureFilter( atlas, TEXTURE_FILTER_POINT );
    RenderTexture2D canvas = LoadRenderTexture( CanvasWidth, CanvasHeight );
    if ( !IsTextureValid( atlas ) || !IsRenderTextureValid( canvas ) )
    {
        std::fprintf( stderr, "VexFactory could not create its rendering resources.\n" );
        UnloadRenderTexture( canvas ); UnloadTexture( atlas ); CloseWindow(); return 1;
    }
    SetTextureFilter( canvas.texture, TEXTURE_FILTER_POINT );
    Simulation sim;
    View view;
    if ( smoke )
    {
        const double start = GetTime();
        for ( int i = 0; i < 60 * 35; ++i ) sim.step();
        view.milliseconds = ( GetTime() - start ) * 1000 / ( 60 * 35 );
        if ( frameLimit == 0 ) frameLimit = 4;
    }
    int frames = 0;
    int canvasFilter = TEXTURE_FILTER_POINT;
    bool screenshotWritten = screenshot.empty();
    while ( !WindowShouldClose() && ( frameLimit == 0 || frames < frameLimit ) )
    {
        if ( IsKeyPressed( KEY_ESCAPE ) && view.pendingReset < 0 ) break;
        const float scale = std::min( static_cast<float>( GetScreenWidth() ) / CanvasWidth, static_cast<float>( GetScreenHeight() ) / CanvasHeight );
        // Fractional downscaling with nearest sampling drops strokes from the
        // pixel font. Keep atlas pixels sharp, but filter the final canvas here.
        const int filter = std::abs( scale - std::round( scale ) ) < 0.001f ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR;
        if ( filter != canvasFilter ) { SetTextureFilter( canvas.texture, filter ); canvasFilter = filter; }
        const Rectangle viewport = { ( GetScreenWidth() - CanvasWidth * scale ) / 2,
            ( GetScreenHeight() - CanvasHeight * scale ) / 2, CanvasWidth * scale, CanvasHeight * scale };
        const Vector2 mouse = smoke ? Vector2{ -100, -100 } : logicalMouse( viewport );
        if ( !smoke ) updateInput( sim, view, mouse );
        if ( !view.paused && view.pendingReset < 0 && !smoke )
        {
            view.accumulator += std::min( GetFrameTime(), 0.25f ) * view.speed;
            const double start = GetTime();
            int ticks = 0;
            while ( view.accumulator >= FixedStep ) { sim.step(); view.accumulator -= FixedStep; ++ticks; }
            if ( ticks ) view.milliseconds = view.milliseconds * 0.9 + ( GetTime() - start ) * 1000 / ticks * 0.1;
        }
        BeginTextureMode( canvas );
        ClearBackground( Background );
        drawGrid( sim, view, atlas, mouse );
        drawUI( sim, view, atlas, mouse );
        EndTextureMode();
        BeginDrawing();
        ClearBackground( BLACK );
        DrawTexturePro( canvas.texture, { 0, 0, static_cast<float>( CanvasWidth ), -static_cast<float>( CanvasHeight ) }, viewport, { 0, 0 }, 0, WHITE );
        EndDrawing();
        ++frames;
        if ( !screenshot.empty() && frames == frameLimit )
        {
            // Export directly so the requested directory and error result survive.
            Image shot = LoadImageFromScreen();
            screenshotWritten = ExportImage( shot, screenshot.c_str() );
            UnloadImage( shot );
        }
    }
    const bool screenshotFailed = !screenshotWritten;
    UnloadRenderTexture( canvas );
    UnloadTexture( atlas );
    CloseWindow();
    if ( screenshotFailed ) { std::fprintf( stderr, "The screenshot was not written.\n" ); return 1; }
    return 0;
}
