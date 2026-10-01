#define NOMINMAX
#include "save_game.h"
#include "interface.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include <cstdio>
#include <cstring>

#ifndef VEX_FACTORY_DEFAULT_ASSET_DIR
#define VEX_FACTORY_DEFAULT_ASSET_DIR "temp/vexfactory/assets"
#endif

using namespace vexfactory;
namespace
{
enum class Screen { Title, Campaign, Briefing, Playing, Pause, Workshop, Help, Result, Ending, Confirm };
enum class Action { Restart, Sandbox, EmptySandbox, Begin };
struct App
{
    Simulation sim;
    Progress progress;
    Screen screen = Screen::Title, previous = Screen::Title;
    Action confirmation = Action::Restart;
    int mission = -1, selected = 0, speed = 1, page = 0, direction = 0;
    int hoverX = -1, hoverY = -1, lastX = -1, lastY = -1;
    Tool tool = Tool::Belt;
    bool active = false, paused = true, quit = false, muted = false, noSave = false, saveBlocked = false, fit = true, strokeSaved = false;
    float scale = 1, accumulator = 0, scroll = 0, panelScroll = 0, toastTime = 0;
    double saveClock = 0;
    std::string toast;
    std::filesystem::path saveDirectory = defaultSaveDirectory();
    Camera2D camera{};
    std::vector<FactoryState> undo;
    Audio audio;
    void message( const std::string& text ) { toast = text; toastTime = 6; }
    void change( Screen next ) { screen = next; scroll = 0; accumulator = 0; lastX = lastY = -1; }
    SaveGame capture()
    {
        SaveGame game; game.progress = progress; game.hasFactory = active; game.mission = mission;
        game.paused = paused; game.muted = muted; game.uiScale = scale;
        if ( active ) game.factory = sim.capture(); return game;
    }
    void save( bool manual = false )
    {
        if ( noSave || saveBlocked ) { saveClock = GetTime(); if ( manual ) message( saveBlocked ? "Previous save preserved. Start a new dispatch to create a fresh profile." : "Saving is disabled for this session." ); return; }
        std::string status;
        if ( !saveGame( saveDirectory, capture(), status ) || manual ) message( status ); saveClock = GetTime();
    }
    void load()
    {
        if ( noSave ) return; std::error_code ec;
        if ( !std::filesystem::exists( saveDirectory / "campaign.sav", ec ) && !std::filesystem::exists( saveDirectory / "campaign.sav.bak", ec ) ) return;
        SaveGame game; std::string status;
        if ( !loadGame( saveDirectory, game, status ) ) { saveBlocked = true; message( "Previous save cannot be read. Its files remain untouched." ); return; }
        progress = game.progress; muted = game.muted; scale = game.uiScale;
        if ( game.hasFactory && sim.restore( game.factory ) ) { active = true; mission = game.mission; paused = true; }
        if ( !status.empty() ) message( status );
    }
    void start( int id, bool emptySandbox = false )
    {
        if ( id < 0 ) sim.reset( !emptySandbox ); else beginMission( sim, id, progress.upgrades );
        mission = id; selected = std::max( 0, id ); active = true; paused = true; fit = true; page = 0;
        tool = Tool::Belt; direction = 0; undo.clear(); panelScroll = 0; saveBlocked = false;
        change( Screen::Playing ); save();
    }
    void resume() { if ( active ) { fit = true; change( sim.phase() == Phase::Won || sim.phase() == Phase::Lost ? Screen::Result : Screen::Playing ); } }
    void request( Action action ) { confirmation = action; previous = screen; change( Screen::Confirm ); }
    void confirm()
    {
        if ( confirmation == Action::Restart ) start( mission );
        else if ( confirmation == Action::Sandbox ) start( -1 );
        else if ( confirmation == Action::EmptySandbox ) start( -1, true );
        else { if ( saveBlocked ) { progress = {}; active = false; } start( selected ); }
    }
    void toggleRun()
    {
        if ( sim.phase() == Phase::Planning ) { sim.launch(); paused = false; undo.clear(); }
        else paused = !paused;
        accumulator = 0;
    }
    void finish()
    {
        if ( screen != Screen::Playing || mission < 0 ) return;
        if ( sim.phase() == Phase::Won ) { const int reward = progress.reward( mission, sim.stars(), sim.stats().elapsed ); if ( reward ) message( TextFormat( "+%d workshop tokens", reward ) ); audio.play( 1, muted ); paused = true; change( Screen::Result ); save(); }
        else if ( sim.phase() == Phase::Lost ) { audio.play( 2, muted ); paused = true; change( Screen::Result ); save(); }
    }
};
void fitCamera( App& app, const Layout& layout )
{
    app.camera.offset = { layout.world.x + layout.world.width / 2, layout.world.y + layout.world.height / 2 };
    if ( app.fit ) { app.camera.target = { Width / 2.0f, Height / 2.0f }; app.camera.zoom = std::min( ( layout.world.width - 24 ) / Width, ( layout.world.height - 24 ) / Height ); }
}
void globalInput( App& app )
{
    const bool command = IsKeyDown( KEY_LEFT_CONTROL ) || IsKeyDown( KEY_RIGHT_CONTROL ) || IsKeyDown( KEY_LEFT_SUPER ) || IsKeyDown( KEY_RIGHT_SUPER );
    if ( command && IsKeyPressed( KEY_S ) ) app.save( true );
    if ( command && ( IsKeyPressed( KEY_EQUAL ) || IsKeyPressed( KEY_KP_ADD ) ) ) app.scale = std::min( 1.4f, app.scale + .1f );
    if ( command && ( IsKeyPressed( KEY_MINUS ) || IsKeyPressed( KEY_KP_SUBTRACT ) ) ) app.scale = std::max( 1.0f, app.scale - .1f );
    if ( IsKeyPressed( KEY_F11 ) ) { ToggleBorderlessWindowed(); app.fit = true; }
    if ( IsKeyPressed( KEY_M ) ) { app.muted = !app.muted; app.message( app.muted ? "Sound muted." : "Sound enabled." ); }
    if ( IsKeyPressed( KEY_F1 ) ) { if ( app.screen == Screen::Help ) app.change( app.previous ); else { app.previous = app.screen; app.change( Screen::Help ); } }
    if ( IsKeyPressed( KEY_ESCAPE ) )
    {
        if ( app.screen == Screen::Playing ) { app.change( Screen::Pause ); app.save(); }
        else if ( app.screen == Screen::Pause ) app.change( Screen::Playing );
        else if ( app.screen == Screen::Confirm || app.screen == Screen::Help || app.screen == Screen::Workshop ) app.change( app.previous );
        else if ( app.screen == Screen::Briefing ) app.change( Screen::Campaign );
        else if ( app.screen == Screen::Title ) app.quit = true;
        else app.change( Screen::Title );
    }
    if ( IsKeyPressed( KEY_ENTER ) )
    {
        if ( app.screen == Screen::Title ) { if ( app.active ) app.resume(); else { app.selected = app.progress.unlocked(); app.change( Screen::Briefing ); } }
        else if ( app.screen == Screen::Campaign ) app.change( Screen::Briefing );
        else if ( app.screen == Screen::Briefing ) { if ( app.active || app.saveBlocked ) app.request( Action::Begin ); else app.start( app.selected ); }
        else if ( app.screen == Screen::Confirm ) app.confirm();
        else if ( app.screen == Screen::Result ) { if ( app.sim.phase() == Phase::Lost ) app.start( app.mission ); else if ( app.mission == MissionCount - 1 ) app.change( Screen::Ending ); else { app.selected = app.mission + 1; app.change( Screen::Briefing ); } }
    }
}
void worldInput( App& app, const Layout& layout )
{
    if ( app.screen != Screen::Playing ) return;
    const Vector2 mouse = GetMousePosition(); const bool inWorld = CheckCollisionPointRec( mouse, rect( layout.world ) );
    constexpr Tool shortcuts[] = { Tool::Belt, Tool::Miner, Tool::Smelter, Tool::Press, Tool::Assembler, Tool::Shipping, Tool::Splitter, Tool::Sorter, Tool::Generator };
    for ( int i = 0; i < 9; ++i ) if ( IsKeyPressed( KEY_ONE + i ) ) { app.tool = shortcuts[i]; app.page = 0; }
    if ( IsKeyPressed( KEY_ZERO ) ) app.tool = Tool::Erase;
    if ( IsKeyPressed( KEY_R ) ) app.direction = ( app.direction + 1 ) % 4;
    if ( IsKeyPressed( KEY_SPACE ) ) app.toggleRun();
    if ( IsKeyPressed( KEY_TAB ) ) app.speed = app.speed == 4 ? 1 : app.speed * 2;
    if ( IsKeyPressed( KEY_HOME ) ) app.fit = true;
    if ( IsKeyPressed( KEY_N ) && app.mission < 0 ) { app.request( Action::EmptySandbox ); return; }
    const bool command = IsKeyDown( KEY_LEFT_CONTROL ) || IsKeyDown( KEY_RIGHT_CONTROL ) || IsKeyDown( KEY_LEFT_SUPER ) || IsKeyDown( KEY_RIGHT_SUPER );
    if ( command && IsKeyPressed( KEY_Z ) && !app.undo.empty() && app.sim.phase() == Phase::Planning )
    { app.sim.restore( app.undo.back() ); app.undo.pop_back(); app.message( "Planning edit undone." ); }
    if ( inWorld )
    {
        const float wheel = GetMouseWheelMove();
        if ( wheel )
        {
            const Vector2 before = GetScreenToWorld2D( mouse, app.camera ); app.fit = false;
            app.camera.zoom = std::clamp( app.camera.zoom * std::pow( 1.15f, wheel ), 12.0f, 96.0f );
            const Vector2 after = GetScreenToWorld2D( mouse, app.camera ); app.camera.target.x += before.x - after.x; app.camera.target.y += before.y - after.y;
        }
        if ( IsMouseButtonDown( MOUSE_BUTTON_MIDDLE ) )
        { app.fit = false; const auto delta = GetMouseDelta(); app.camera.target.x -= delta.x / app.camera.zoom; app.camera.target.y -= delta.y / app.camera.zoom; }
        Vector2 point = GetScreenToWorld2D( mouse, app.camera ); app.hoverX = static_cast<int>( std::floor( point.x ) ); app.hoverY = static_cast<int>( std::floor( point.y ) );
    }
    float panX = ( IsKeyDown( KEY_D ) ? 1.0f : 0 ) - ( IsKeyDown( KEY_A ) ? 1.0f : 0 );
    float panY = ( IsKeyDown( KEY_S ) && !command ? 1.0f : 0 ) - ( IsKeyDown( KEY_W ) ? 1.0f : 0 );
    if ( panX || panY ) { app.fit = false; app.camera.target.x += panX * GetFrameTime() * 12; app.camera.target.y += panY * GetFrameTime() * 12; }
    if ( IsKeyPressed( KEY_E ) && Simulation::inside( app.hoverX, app.hoverY ) )
    {
        const auto e = app.sim.at( app.hoverX, app.hoverY );
        if ( e != VECS_INVALID_ENTITY ) { app.tool = vecsGet<Building>( app.sim.world(), e )->tool; app.direction = vecsGet<Cell>( app.sim.world(), e )->direction; }
        app.page = 2; app.panelScroll = 0;
    }
    if ( IsKeyPressed( KEY_F ) ) app.sim.cycleFilter( app.hoverX, app.hoverY );
    if ( !IsMouseButtonDown( MOUSE_BUTTON_LEFT ) && !IsMouseButtonDown( MOUSE_BUTTON_RIGHT ) ) { app.lastX = app.lastY = -1; app.strokeSaved = false; }
    if ( !inWorld || !Simulation::inside( app.hoverX, app.hoverY ) || IsMouseButtonDown( MOUSE_BUTTON_MIDDLE ) ) return;
    if ( ( IsKeyDown( KEY_LEFT_ALT ) || IsKeyDown( KEY_RIGHT_ALT ) ) && IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) ) { app.page = 2; app.panelScroll = 0; return; }
    const bool erase = IsMouseButtonDown( MOUSE_BUTTON_RIGHT );
    const bool paint = IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) || ( IsMouseButtonDown( MOUSE_BUTTON_LEFT ) && ( app.tool == Tool::Belt || app.tool == Tool::Erase ) );
    if ( !erase && !paint ) return;
    if ( app.lastX == app.hoverX && app.lastY == app.hoverY && !IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) && !IsMouseButtonPressed( MOUSE_BUTTON_RIGHT ) ) return;
    FactoryState before; const bool canUndo = app.sim.phase() == Phase::Planning && !app.strokeSaved;
    if ( canUndo ) before = app.sim.capture(); bool changed = false;
    const auto apply = [&]( int x, int y )
    {
        const Tool tool = erase ? Tool::Erase : app.tool; const auto existing = app.sim.at( x, y );
        if ( existing != VECS_INVALID_ENTITY && tool != Tool::Erase && vecsGet<Building>( app.sim.world(), existing )->tool != tool && !( IsKeyDown( KEY_LEFT_SHIFT ) || IsKeyDown( KEY_RIGHT_SHIFT ) ) )
        { app.message( "Occupied. Shift-click replaces a part; right-click demolishes it." ); return; }
        std::string reason;
        if ( !app.sim.canPlace( x, y, tool, &reason ) ) { app.message( reason ); return; }
        if ( app.sim.place( x, y, tool, app.direction ) ) changed = true;
    };
    // Fill cells skipped by a fast mouse stroke; don't depend on frame rate.
    if ( ( app.tool == Tool::Belt || erase || app.tool == Tool::Erase ) && Simulation::inside( app.lastX, app.lastY ) )
    {
        int x = app.lastX, y = app.lastY;
        while ( x != app.hoverX ) { x += x < app.hoverX ? 1 : -1; apply( x, y ); }
        while ( y != app.hoverY ) { y += y < app.hoverY ? 1 : -1; apply( x, y ); }
    }
    else apply( app.hoverX, app.hoverY );
    if ( changed )
    {
        if ( canUndo ) { if ( app.undo.size() == 24 ) app.undo.erase( app.undo.begin() ); app.undo.push_back( std::move( before ) ); app.strokeSaved = true; }
        app.audio.play( 0, app.muted );
    }
    else app.audio.play( 2, app.muted );
    app.lastX = app.hoverX; app.lastY = app.hoverY;
}
void drawFactory( App& app, const Layout& layout, Texture2D atlas )
{
    Rectangle viewport = rect( layout.world ); DrawRectangleRec( viewport, { 36, 41, 45, 255 } );
    BeginScissorMode( static_cast<int>( viewport.x ), static_cast<int>( viewport.y ), static_cast<int>( viewport.width ), static_cast<int>( viewport.height ) );
    BeginMode2D( app.camera );
    for ( int y = 0; y < Height; ++y ) for ( int x = 0; x < Width; ++x )
    {
        Rectangle cell{ static_cast<float>( x ), static_cast<float>( y ), 1, 1 };
        if ( app.sim.blocked( x, y ) )
        {
            DrawRectangleRec( cell, { 28, 45, 58, 255 } );
            for ( int n = 0; n < 3; ++n ) DrawLineEx( { x + .15f, y + .25f + n * .2f }, { x + .8f, y + .25f + n * .2f }, .025f, { 52, 85, 106, 255 } );
        }
        else
        {
            sprite( atlas, ( x * 7 + y * 11 ) % 13 == 0 ? 1 : 0, cell, 0, { 174, 161, 147, 255 } );
            DrawRectangleLinesEx( cell, .02f, { 49, 46, 45, 35 } );
            if ( app.sim.scenario().ore[Simulation::index( x, y )] )
            {
                sprite( atlas, 96, { x + .1f, y + .1f, .8f, .8f }, 0, app.sim.oreAt( x, y ) ? WHITE : Color{ 90, 90, 90, 255 } );
                DrawRectangleLinesEx( { x + .05f, y + .05f, .9f, .9f }, .06f, app.sim.oreAt( x, y ) ? ProductColors[1] : Muted );
            }
        }
    }
    app.sim.buildings( [&]( vecsEntity e, Cell& cell, Building& b )
    {
        const float x = static_cast<float>( cell.x ), y = static_cast<float>( cell.y );
        if ( isTransport( b.tool ) )
        {
            sprite( atlas, 26, { x, y, 1, 1 }, cell.direction * 90.0f );
            if ( const auto* router = vecsGet<Router>( app.sim.world(), e ) )
            {
                DrawRectangleRec( { x + .1f, y + .1f, .8f, .8f }, Fade( router->splitter ? Gold : Mint, .22f ) );
                const int side = ( cell.direction + 3 ) % 4;
                arrow( { x + .5f + DX[cell.direction] * .3f, y + .5f + DY[cell.direction] * .3f }, cell.direction, router->splitter ? Gold : Mint, .12f );
                arrow( { x + .5f + DX[side] * .3f, y + .5f + DY[side] * .3f }, side, router->splitter ? Gold : Muted, .12f );
                if ( !router->splitter ) sprite( atlas, ItemTiles[static_cast<int>( router->filter )], { x + .32f, y + .32f, .36f, .36f } );
            }
            return;
        }
        DrawRectangleRec( { x + .03f, y + .03f, .94f, .94f }, { 44, 54, 64, 255 } );
        sprite( atlas, ToolTiles[static_cast<int>( b.tool )], { x + .05f, y + .05f, .9f, .9f } );
        if ( b.tool != Tool::Shipping && b.tool != Tool::Generator )
        { DrawCircleV( { x + .5f + DX[cell.direction] * .35f, y + .5f + DY[cell.direction] * .35f }, .18f, Background ); arrow( { x + .5f + DX[cell.direction] * .35f, y + .5f + DY[cell.direction] * .35f }, cell.direction, Mint, .075f ); }
        if ( const auto* p = vecsGet<Processor>( app.sim.world(), e ) )
        {
            DrawRectangleRec( { x + .1f, y + .85f, .8f, .1f }, Background );
            DrawRectangleRec( { x + .1f, y + .85f, .8f * std::min( 1.0f, p->progress / app.sim.workDuration( p->recipe ) ), .1f }, p->ready ? Coral : Mint );
            const auto* inv = vecsGet<Inventory>( app.sim.world(), e );
            for ( uint32_t i = 0; i < inv->stored; ++i ) DrawRectangleRec( { x + .06f + i * .13f, y + .05f, .08f, .08f }, Gold );
            for ( uint32_t i = 0; i < inv->plates; ++i ) DrawRectangleRec( { x + .06f + i * .13f, y + .19f, .08f, .08f }, ProductColors[1] );
        }
        if ( b.tool == Tool::Generator ) { DrawCircleV( { x + .2f, y + .2f }, .1f, Gold ); }
        if ( b.tool == Tool::Shipping )
        {
            const auto* dock = vecsGet<Dock>( app.sim.world(), e ); bool open = !dock || dock->mask;
            if ( dock && app.mission >= 0 ) { open = false; for ( int i = 0; i < ItemCount; ++i ) open |= ( dock->mask & ( 1u << i ) ) && app.sim.stats().delivered[i] < app.sim.scenario().goals[i]; }
            DrawCircleV( { x + .15f, y + .15f }, .08f, open ? Mint : dock && dock->mask ? Gold : Muted );
            if ( dock ) for ( int i = 0; i < ItemCount; ++i ) if ( dock->mask & ( 1u << i ) ) { sprite( atlas, ItemTiles[i], { x + .6f, y + .1f, .35f, .35f } ); break; }
        }
    } );
    app.sim.parcels( [&]( vecsEntity, Parcel& parcel, Transit& t )
    {
        const float x = t.fromX + ( t.x - t.fromX ) * t.progress + .5f, y = t.fromY + ( t.y - t.fromY ) * t.progress + .5f;
        DrawCircleV( { x, y + .2f }, .22f, { 0, 0, 0, 55 } );
        sprite( atlas, ItemTiles[static_cast<int>( parcel.kind )], { x - .31f, y - .31f, .62f, .62f } );
        if ( t.progress >= 1 ) DrawRectangleLinesEx( { x - .34f, y - .34f, .68f, .68f }, .035f, Coral );
    } );
    if ( app.screen == Screen::Playing && Simulation::inside( app.hoverX, app.hoverY ) && CheckCollisionPointRec( GetMousePosition(), viewport ) )
    {
        const bool legal = app.sim.canPlace( app.hoverX, app.hoverY, app.tool ); const Color color = legal ? Mint : Coral;
        Rectangle cell{ static_cast<float>( app.hoverX ), static_cast<float>( app.hoverY ), 1, 1 };
        DrawRectangleRec( cell, Fade( color, .22f ) ); DrawRectangleLinesEx( cell, .06f, color );
        sprite( atlas, ToolTiles[static_cast<int>( app.tool )], cell, app.tool == Tool::Belt ? app.direction * 90.0f : 0, Fade( WHITE, .4f ) );
    }
    EndMode2D(); EndScissorMode(); DrawRectangleLinesEx( viewport, 1, Border );
}
void statCard( Ui& ui, Rectangle r, const char* name, const std::string& value, Color color )
{
    ui.box( r ); ui.text( name, r.x + 12, r.y + 8, ui.small(), Muted, r.width - 24 );
    ui.text( value, r.x + 12, r.y + 31 * ui.scale, 30 * ui.scale, color, r.width - 24 );
}
float paragraph( Ui& ui, const std::string& text, float x, float y, float width, Color color = Ink ) { return ui.wrap( text, x, y, width, ui.body(), color ) + 14 * ui.scale; }
void drawHud( App& app, Ui& ui, const Layout& l, Texture2D atlas )
{
    const auto& stats = app.sim.stats();
    ui.text( app.mission < 0 ? "VexFactory / Free build" : TextFormat( "Chapter %02d / The Last Freight", app.mission + 1 ), 20, 10, ui.small(), Mint, l.width * .47f - 40 );
    ui.text( app.mission < 0 ? "Sandbox" : missions()[app.mission].title, 20, 36 * app.scale, 27 * app.scale, Ink, l.width * .47f - 40 );
    const char* state = app.sim.phase() == Phase::Planning ? "PLANNING / clock stopped" : app.paused ? "PAUSED / clock stopped" : "PRODUCTION LIVE";
    ui.text( state, 20, 77 * app.scale, ui.small(), app.paused ? Gold : Mint, l.width * .47f - 40 );
    const float start = l.width * .48f, cardWidth = ( l.width - start - 36 ) / 3;
    const float height = 88 * app.scale;
    statCard( ui, { start, 10, cardWidth, height }, app.mission < 0 ? "Freight/min" : "Credits", app.mission < 0 ? TextFormat( "%.1f", stats.perMinute ) : TextFormat( "%d", stats.credits ), Mint );
    statCard( ui, { start + cardWidth + 10, 10, cardWidth, height }, "Power", app.mission < 0 ? "Free" : TextFormat( "%d/%d", stats.powerUsed, stats.powerLimit ), Gold );
    const int seconds = std::max( 0, static_cast<int>( std::ceil( app.sim.scenario().deadline - stats.elapsed ) ) );
    statCard( ui, { start + ( cardWidth + 10 ) * 2, 10, cardWidth, height }, app.mission < 0 ? "Sim time" : "Departure", TextFormat( "%d:%02d", app.mission < 0 ? static_cast<int>( stats.elapsed ) / 60 : seconds / 60, app.mission < 0 ? static_cast<int>( stats.elapsed ) % 60 : seconds % 60 ), app.mission >= 0 && seconds < 30 ? Coral : Ink );
    ui.box( rect( l.panel ) );
    const char* tabs[] = { "Build", "Orders", "Inspect" };
    for ( int i = 0; i < 3; ++i ) if ( ui.button( { l.panelTabs.x + i * l.panelTabs.width / 3, l.panelTabs.y, l.panelTabs.width / 3 - 4, l.panelTabs.height }, tabs[i], app.page == i ) ) { app.page = i; app.panelScroll = 0; }
    const Rectangle body = rect( l.panelBody );
    if ( CheckCollisionPointRec( ui.mouse, body ) ) app.panelScroll = std::max( 0.0f, app.panelScroll - GetMouseWheelMove() * 46 * app.scale );
    ui.beginClip( body ); float y = body.y + 4 - app.panelScroll, x = body.x, width = body.width - 10;
    if ( app.page == 0 )
    {
        constexpr Tool tools[] = { Tool::Belt, Tool::Miner, Tool::Smelter, Tool::Press, Tool::Assembler, Tool::Shipping, Tool::Splitter, Tool::Sorter, Tool::Generator, Tool::Erase };
        constexpr const char* keys[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };
        const float gap = 8, cw = ( width - gap ) / 2, rowHeight = 72 * app.scale;
        for ( int i = 0; i < 10; ++i )
        {
            const Tool t = tools[i]; const bool enabled = app.mission < 0 || ( ( app.sim.scenario().unlocked & toolBit( t ) ) && t != Tool::Shipping );
            Rectangle button{ x + ( i % 2 ) * ( cw + gap ), y + ( i / 2 ) * ( rowHeight + gap ), cw, rowHeight };
            const bool hover = ui.hit( button ); ui.box( button, app.tool == t ? Color{ 38, 73, 65, 255 } : hover && enabled ? Color{ 46, 60, 72, 255 } : Background );
            sprite( atlas, ToolTiles[static_cast<int>( t )], { button.x + 8, button.y + 9, 28 * app.scale, 28 * app.scale }, t == Tool::Belt ? app.direction * 90.0f : 0, enabled ? WHITE : Color{ 140, 140, 140, 255 } );
            ui.text( toolName( t ), button.x + 42 * app.scale, button.y + 8, ui.small(), enabled ? Ink : Muted, cw - 48 * app.scale );
            ui.text( app.mission < 0 ? std::string( keys[i] ) + " / free" : !enabled ? "Locked" : TextFormat( "%s / $%d  %dP", keys[i], costFor( t ), powerFor( t ) ), button.x + 10, button.y + 42 * app.scale, ui.small(), app.tool == t ? Mint : Muted, cw - 18 );
            if ( hover && enabled && IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) ) { app.tool = t; app.audio.play( 0, app.muted ); }
        }
        y += 5 * ( rowHeight + gap ) + 8;
        constexpr const char* directions[] = { "East", "South", "West", "North" };
        y += paragraph( ui, std::string( "Output: " ) + directions[app.direction] + ". R rotates the brush.", x, y, width, Gold );
        std::string recipe = app.tool == Tool::Miner ? "Miners need ore deposits. Their output needs a conveyor." : app.tool == Tool::Smelter ? "1 ore -> 1 plate" : app.tool == Tool::Press ? ( app.mission < 0 ? "1 plate -> 1 gear" : "2 plates -> 1 gear" ) : app.tool == Tool::Assembler ? ( app.mission < 0 ? "1 gear -> 1 engine" : "2 gears + 1 plate -> 1 engine. Feed both inputs from different sides." ) : app.tool == Tool::Splitter ? "Forward and left outputs alternate. A blocked exit sends cargo through the other exit." : app.tool == Tool::Sorter ? "Selected cargo goes forward; other cargo goes left. F changes the filter under the pointer." : app.tool == Tool::Generator ? "+12 power. You cannot remove power committed to machines." : app.tool == Tool::Erase ? "Demolition scraps cargo. Purchased parts refund 70-85%; free prefabs refund nothing." : "Drag straight conveyor runs. Items follow arrows. Right-click demolishes; Shift-click replaces.";
        y += paragraph( ui, recipe, x, y, width );
        if ( app.sim.phase() == Phase::Planning ) y += paragraph( ui, "Ctrl/Cmd+Z undoes planning edits. Launch clears undo history.", x, y, width, Muted );
    }
    else if ( app.page == 1 )
    {
        y += paragraph( ui, app.mission < 0 ? "Sandbox freight" : "Required cargo", x, y, width, Mint );
        for ( int i = 0; i < ItemCount; ++i ) if ( app.mission < 0 || app.sim.scenario().goals[i] )
        {
            sprite( atlas, ItemTiles[i], { x, y, 30 * app.scale, 30 * app.scale } );
            ui.text( itemName( static_cast<Item>( i ) ), x + 42 * app.scale, y, ui.body(), ProductColors[i], width - 42 * app.scale );
            ui.text( app.mission < 0 ? TextFormat( "%u shipped", stats.delivered[i] ) : TextFormat( "%u / %u", stats.delivered[i], app.sim.scenario().goals[i] ), x + 42 * app.scale, y + 27 * app.scale, ui.small(), Ink );
            if ( app.mission >= 0 ) ui.text( TextFormat( "+$%d each", priceFor( static_cast<Item>( i ) ) ), x + width - 105 * app.scale, y + 27 * app.scale, ui.small(), Gold, 105 * app.scale );
            if ( app.mission >= 0 ) { ui.box( { x, y + 56 * app.scale, width, 8 * app.scale }, Background ); DrawRectangleRec( { x, y + 56 * app.scale, width * std::min( 1.0f, static_cast<float>( stats.delivered[i] ) / app.sim.scenario().goals[i] ), 8 * app.scale }, ProductColors[i] ); }
            y += 80 * app.scale;
        }
        y += paragraph( ui, TextFormat( "%u items moving / %u waiting", stats.items, stats.waiting ), x, y, width, Gold );
        if ( app.mission >= 0 )
        {
            y += paragraph( ui, TextFormat( "%u ore remains in deposits. %u ore-equivalent units scrapped.", app.sim.oreLeft(), stats.scrapped ), x, y, width );
            y += paragraph( ui, missions()[app.mission].hint, x, y, width, Muted );
        }
        else y += paragraph( ui, "Campaign mode adds budgets, finite deposits, power, and multi-input recipes. This sandbox keeps the original one-to-one recipes.", x, y, width, Muted );
    }
    else
    {
        const int hx = app.hoverX, hy = app.hoverY;
        if ( !Simulation::inside( hx, hy ) ) y += paragraph( ui, "Point at a tile, then press E or Alt-click to inspect it.", x, y, width );
        else
        {
            y += paragraph( ui, TextFormat( "Tile %d, %d", hx + 1, hy + 1 ), x, y, width, Mint );
            if ( app.sim.blocked( hx, hy ) ) y += paragraph( ui, "Damaged floor. Route cargo around this obstruction.", x, y, width );
            if ( app.sim.scenario().ore[Simulation::index( hx, hy )] ) y += paragraph( ui, TextFormat( "Deposit: %u ore remaining.", app.sim.oreAt( hx, hy ) ), x, y, width, ProductColors[1] );
            const auto e = app.sim.at( hx, hy );
            if ( e != VECS_INVALID_ENTITY )
            {
                const auto& b = *vecsGet<Building>( app.sim.world(), e ); y += paragraph( ui, toolName( b.tool ), x, y, width );
                if ( const auto* p = vecsGet<Processor>( app.sim.world(), e ) )
                {
                    const auto* inv = vecsGet<Inventory>( app.sim.world(), e );
                    y += paragraph( ui, TextFormat( "%s: %u / 4 buffered", itemName( inputFor( p->recipe ) ), inv->stored ), x, y, width, Gold );
                    if ( p->recipe == Tool::Assembler && app.mission >= 0 ) y += paragraph( ui, TextFormat( "Extra plates: %u / 4 buffered", inv->plates ), x, y, width, ProductColors[1] );
                    y += paragraph( ui, p->ready ? "Output blocked. Check the next conveyor and its dock quota." : p->working ? TextFormat( "Processing: %.1f / %.1f seconds", p->progress, app.sim.workDuration( p->recipe ) ) : "Waiting for recipe inputs. Check quantities and incoming cargo types.", x, y, width, p->ready ? Coral : Mint );
                }
                if ( const auto* dock = vecsGet<Dock>( app.sim.world(), e ) )
                {
                    y += paragraph( ui, dock->mask ? "Protected freight dock. Requested cargo:" : "This dock is closed for this dispatch.", x, y, width );
                    for ( int i = 0; i < ItemCount; ++i ) if ( dock->mask & ( 1u << i ) ) y += paragraph( ui, itemName( static_cast<Item>( i ) ), x, y, width, ProductColors[i] );
                }
                if ( const auto* r = vecsGet<Router>( app.sim.world(), e ) ) y += paragraph( ui, r->splitter ? "Alternating forward/left. Either exit can relieve a jam." : std::string( "Filter: " ) + itemName( r->filter ) + " forward. Other cargo left. Press F to change.", x, y, width, Gold );
                if ( app.mission >= 0 && !b.locked ) y += paragraph( ui, TextFormat( "Demolition returns %d credits and scraps all contents.", app.sim.refundFor( b ) ), x, y, width, Muted );
            }
            else if ( !app.sim.blocked( hx, hy ) ) y += paragraph( ui, "Open floor. Place a part here.", x, y, width );
        }
    }
    const float content = y - body.y + app.panelScroll;
    ui.endClip(); app.panelScroll = std::clamp( app.panelScroll, 0.0f, std::max( 0.0f, content - body.height + 12 ) );
    if ( content > body.height )
    {
        const float thumb = std::max( 28.0f, body.height * body.height / content );
        DrawRectangleRec( { l.panel.x + l.panel.width - 6, body.y + ( body.height - thumb ) * app.panelScroll / std::max( 1.0f, content - body.height + 12 ), 3, thumb }, Mint );
    }
    const float gap = 10, bw = ( l.width - 32 - gap * 4 ) / 5;
    Rectangle b{ 16, l.footer.y + 8, bw, l.footer.height - 16 };
    if ( ui.button( b, app.sim.phase() == Phase::Planning ? "Launch [Space]" : app.paused ? "Resume [Space]" : "Pause [Space]", !app.paused ) ) app.toggleRun(); b.x += bw + gap;
    if ( ui.button( b, TextFormat( "Speed %dx [Tab]", app.speed ) ) ) app.speed = app.speed == 4 ? 1 : app.speed * 2; b.x += bw + gap;
    if ( ui.button( b, "Fit [Home]" ) ) app.fit = true; b.x += bw + gap;
    if ( ui.button( b, "Save" ) ) app.save( true ); b.x += bw + gap;
    if ( ui.button( b, "Menu [Esc]" ) ) { app.change( Screen::Pause ); app.save(); }
}
void menuBackground( Texture2D atlas, const Layout& l )
{
    for ( int row = 0; row < 4; ++row )
    {
        const float y = l.height * .28f + row * 100;
        for ( int x = static_cast<int>( l.width * .45f ); x < l.width + 40; x += 48 ) sprite( atlas, 26, { static_cast<float>( x ), y, 48, 48 }, 0, { 100, 120, 130, 50 } );
        sprite( atlas, ItemTiles[row], { l.width * .7f + static_cast<float>( std::sin( GetTime() * .5 + row ) * 65 ), y + 7, 34, 34 }, 0, { 180, 200, 210, 100 } );
    }
}
void menuHeading( Ui& ui, const Layout& l, const char* eyebrow, const char* title )
{
    ui.text( eyebrow, 28, 20, ui.small(), Mint, l.width - 56 );
    ui.text( title, 28, 48 * ui.scale, 36 * ui.scale, Ink, l.width - 56 );
}
Rectangle menuBody( const Layout& l ) { return { 28, 108 * l.scale, l.width - 56, l.height - 186 * l.scale }; }
void scrollInput( App& app, Ui& ui, Rectangle body ) { if ( CheckCollisionPointRec( ui.mouse, body ) ) app.scroll = std::max( 0.0f, app.scroll - GetMouseWheelMove() * 48 * app.scale ); ui.beginClip( body ); }
void endScroll( App& app, Ui& ui, Rectangle body, float endY )
{
    const float height = endY - body.y + app.scroll; ui.endClip(); app.scroll = std::clamp( app.scroll, 0.0f, std::max( 0.0f, height - body.height ) );
    if ( height > body.height )
    {
        DrawRectangleRec( { body.x, body.y + body.height - 28 * app.scale, body.width, 28 * app.scale }, Background );
        ui.text( "Scroll for more", body.x + body.width - 150 * app.scale, body.y + body.height - 28 * app.scale, ui.small(), Mint, 150 * app.scale );
    }
}
void drawMenus( App& app, Ui& ui, const Layout& l, Texture2D atlas )
{
    menuBackground( atlas, l );
    const float bottom = l.height - 62 * app.scale, buttonHeight = 46 * app.scale;
    if ( app.screen == Screen::Title )
    {
        menuHeading( ui, l, "THE LAST FREIGHT", "VexFactory" );
        ui.wrap( "The line is broken. The settlements are waiting. Build the factory that brings everyone home.", 28, 112 * app.scale, std::min( l.width - 56, 640 * app.scale ), ui.body(), Muted );
        const float cw = std::min( 310 * app.scale, ( l.width - 76 ) / 2 ), gap = 16;
        const float top = std::min( std::max( 210 * app.scale, l.height * .39f ), bottom - 3 * ( buttonHeight + 12 ) - 26 - ui.small() );
        const char* titles[] = { "Continue factory [Enter]", "Campaign", "Sandbox", "Workshop", "How to play", "Quit" };
        for ( int i = 0; i < 6; ++i )
        {
            Rectangle r{ 28 + ( i % 2 ) * ( cw + gap ), top + ( i / 2 ) * ( buttonHeight + 12 ), cw, buttonHeight };
            if ( ui.button( r, titles[i], i == 1, i != 0 || app.active ) )
            {
                if ( i == 0 ) app.resume();
                if ( i == 1 ) app.change( Screen::Campaign );
                if ( i == 2 ) { if ( app.active ) app.request( Action::Sandbox ); else app.start( -1 ); }
                if ( i == 3 ) { app.previous = Screen::Title; app.change( Screen::Workshop ); }
                if ( i == 4 ) { app.previous = Screen::Title; app.change( Screen::Help ); }
                if ( i == 5 ) app.quit = true;
            }
        }
        ui.text( TextFormat( "%d / 30 stars  |  %d workshop tokens", app.progress.totalStars(), app.progress.tokens ), 28, top + 3 * ( buttonHeight + 12 ) + 10, ui.small(), Gold, l.width - 56 );
        if ( ui.button( { 28, bottom, 210 * app.scale, buttonHeight }, TextFormat( "Text %d%%", static_cast<int>( app.scale * 100 + .5f ) ) ) ) app.scale = app.scale < 1.1f ? 1.15f : app.scale < 1.25f ? 1.3f : 1;
        if ( ui.button( { 248 * app.scale, bottom, 180 * app.scale, buttonHeight }, app.muted ? "Sound: off" : "Sound: on" ) ) app.muted = !app.muted;
        ui.text( "Native UI / Retina support / Ctrl or Cmd +/- changes text", 28, l.height - 20, 18, Muted, l.width - 56 );
    }
    else if ( app.screen == Screen::Campaign )
    {
        menuHeading( ui, l, "LUMEN FREIGHT LINE", "Campaign dispatch board" );
        Rectangle body = menuBody( l ); scrollInput( app, ui, body );
        const float gap = 14, cw = ( body.width - gap ) / 2, rh = 88 * app.scale;
        for ( int id = 0; id < MissionCount; ++id )
        {
            Rectangle r{ body.x + ( id % 2 ) * ( cw + gap ), body.y + ( id / 2 ) * ( rh + gap ) - app.scroll, cw, rh };
            const bool enabled = id <= app.progress.unlocked(); ui.box( r, id == app.selected ? Color{ 34, 61, 57, 255 } : Panel );
            ui.text( TextFormat( "%02d / %s", id + 1, enabled ? missions()[id].title : "Awaiting clearance" ), r.x + 14, r.y + 12, ui.body(), enabled ? Ink : Muted, r.width - 28 );
            ui.text( !enabled ? "Complete the previous dispatch" : app.progress.stars[id] ? TextFormat( "%s  /  best %.0fs", app.progress.stars[id] == 3 ? "***" : app.progress.stars[id] == 2 ? "**" : "*", app.progress.bestTime[id] ) : missions()[id].place, r.x + 14, r.y + 48 * app.scale, ui.small(), enabled ? Gold : Muted, r.width - 28 );
            if ( ui.hit( r ) && enabled && IsMouseButtonPressed( MOUSE_BUTTON_LEFT ) ) { app.selected = id; app.change( Screen::Briefing ); break; }
        }
        endScroll( app, ui, body, body.y + 5 * ( rh + gap ) - app.scroll );
        if ( ui.button( { 28, bottom, 180 * app.scale, buttonHeight }, "Back" ) ) app.change( Screen::Title );
        if ( ui.button( { 220 * app.scale, bottom, 250 * app.scale, buttonHeight }, "Workshop" ) ) { app.previous = Screen::Campaign; app.change( Screen::Workshop ); }
    }
    else if ( app.screen == Screen::Briefing )
    {
        const auto& mission = missions()[app.selected]; menuHeading( ui, l, mission.speaker, mission.title );
        const Rectangle body = menuBody( l ); scrollInput( app, ui, body ); float y = body.y - app.scroll;
        const float width = std::min( body.width, 1050.0f ); y += paragraph( ui, mission.briefing, body.x, y, width );
        y += paragraph( ui, TextFormat( "Budget %d credits  /  Base power %d  /  Dispatch allowance %d seconds", mission.credits, mission.power, mission.deadline ), body.x, y, width, Gold );
        std::string order = "Required: "; for ( int i = 0; i < ItemCount; ++i ) if ( mission.goals[i] ) order += std::to_string( mission.goals[i] ) + " " + itemName( static_cast<Item>( i ) ) + "  ";
        y += paragraph( ui, order, body.x, y, width, Mint );
        int income = 0; for ( int i = 0; i < ItemCount; ++i ) income += mission.goals[i] * priceFor( static_cast<Item>( i ) );
        y += paragraph( ui, TextFormat( "Order revenue: %d credits, paid as required cargo arrives. Surplus earns nothing.", income ), body.x, y, width, Gold );
        y += paragraph( ui, mission.hint, body.x, y, width, Muted );
        y += paragraph( ui, TextFormat( "Bonus stars: finish within %d seconds; no scrap and net construction cost at most %d credits. You can plan and pause without spending dispatch time.", mission.parTime, mission.parCost ), body.x, y, width, Muted );
        endScroll( app, ui, body, y );
        if ( ui.button( { 28, bottom, 180 * app.scale, buttonHeight }, "Back" ) ) app.change( Screen::Campaign );
        if ( ui.button( { l.width - 300 * app.scale - 28, bottom, 300 * app.scale, buttonHeight }, "Begin dispatch [Enter]", true ) ) { if ( app.active || app.saveBlocked ) app.request( Action::Begin ); else app.start( app.selected ); }
    }
    else if ( app.screen == Screen::Workshop )
    {
        menuHeading( ui, l, "VALE'S WORKSHOP", TextFormat( "Research / %d tokens", app.progress.tokens ) );
        Rectangle body = menuBody( l ); scrollInput( app, ui, body ); float y = body.y - app.scroll;
        const char* names[] = { "Conveyor bearings", "Precision tooling", "Recovery crews" };
        const char* descriptions[] = { "+25% conveyor speed per tier. Shorter queues and faster transport.", "+15% processing rate per tier, with a small miner-rate increase.", "+5 percentage points of demolition refund per tier, up to 85%. Scrapped cargo is still lost." };
        for ( int track = 0; track < 3; ++track )
        {
            const float content = ui.wrap( descriptions[track], body.x + 16, y + 46 * app.scale, body.width - 260 * app.scale, ui.body(), Muted, false );
            Rectangle r{ body.x, y, body.width, std::max( 124 * app.scale, content + 68 * app.scale ) }; ui.box( r );
            ui.text( std::string( names[track] ) + " / tier " + std::to_string( app.progress.upgrades[track] ) + "/3", r.x + 16, r.y + 14, ui.body(), Mint, r.width - 250 * app.scale );
            ui.wrap( descriptions[track], r.x + 16, r.y + 46 * app.scale, r.width - 260 * app.scale, ui.body(), Muted );
            const int price = app.progress.upgradeCost( track );
            if ( ui.button( { r.x + r.width - 212 * app.scale, r.y + 30 * app.scale, 196 * app.scale, 52 * app.scale }, price ? TextFormat( "Buy / %d tokens", price ) : "Fully upgraded", false, price && app.progress.tokens >= price ) ) { app.progress.purchase( track ); app.save(); app.audio.play( 0, app.muted ); }
            y += r.height + 16;
        }
        y += paragraph( ui, "First clearance earns two tokens plus your stars. Better medals earn only the difference, so repeating a result cannot farm tokens. Upgrades apply to the next dispatch, not a factory already in progress. Every chapter is solvable without them.", body.x, y, body.width, Gold );
        endScroll( app, ui, body, y ); if ( ui.button( { 28, bottom, 180 * app.scale, buttonHeight }, "Back" ) ) app.change( app.previous );
    }
    else if ( app.screen == Screen::Help )
    {
        menuHeading( ui, l, "ENGINEER'S FIELD NOTES", "How to play" ); Rectangle body = menuBody( l ); scrollInput( app, ui, body ); float y = body.y - app.scroll;
        const char* notes[] = {
            "Plan first. Miners must sit on a blue ore deposit in campaign mode. The dispatch clock starts only after Launch. Pause at any time to inspect a jam or rebuild.",
            "Recipes: 1 ore -> 1 plate; 2 plates -> 1 gear; 2 gears + 1 plate -> 1 engine. Each machine output must face an adjacent conveyor. Inputs can arrive from any side. The sandbox retains the original one-to-one recipes.",
            "Power is reserved by installed machines: miners 2, smelters 3, presses 4, assemblers 5. Generators provide 12. You cannot demolish a generator while its power is in use.",
            "Splitters alternate forward and left, using the other port when one is blocked. Sorters send their selected product forward and other cargo left. Point at a sorter and press F to change its filter.",
            "Docks are protected. Their cargo icon shows the requested product. Wrong cargo or a filled quota blocks incoming belts. Only needed deliveries pay credits; there is no surplus-income exploit.",
            "1-9 selects parts. 0 demolishes. R rotates the brush. E picks and inspects a tile. Alt-click inspects without building. Right-click demolishes. Shift-click replaces a different part. Drag to paint conveyors.",
            "Space launches or pauses. Tab cycles production speed. Wheel over the floor zooms; middle-drag or WASD pans. Home fits the floor. Wheel over a panel scrolls its contents. The interface never shrinks with world zoom.",
            "Ctrl/Cmd+Z undoes planning edits. Undo ends at launch. Ctrl/Cmd+S saves. Ctrl/Cmd +/- changes text size. M mutes sound. Esc opens the menu. F1 opens these notes.",
            "Demolition refunds 70% of the price you actually paid, rising to 85% with research. Ore in conveyors and machine buffers is scrapped. A failed dispatch can always be retried; campaign rewards and completed chapters remain.",
            "Saves live in your operating system's user-data directory, not the checkout. The game writes a backup, resumes a saved factory paused, and reports unreadable saves without discarding your running factory. Assets are still opt-in and stay outside Git."
        };
        for ( const char* note : notes ) y += paragraph( ui, note, body.x, y, body.width );
        endScroll( app, ui, body, y ); if ( ui.button( { 28, bottom, 180 * app.scale, buttonHeight }, "Back" ) ) app.change( app.previous );
    }
    else if ( app.screen == Screen::Pause )
    {
        menuHeading( ui, l, "DISPATCH CLOCK STOPPED", "Factory menu" );
        const float w = std::min( 360 * app.scale, ( l.width - 76 ) / 2 ), top = 128 * app.scale;
        const char* names[] = { "Resume factory", "Save game", "Workshop", "Restart dispatch", "Campaign board", "How to play" };
        for ( int i = 0; i < 6; ++i ) if ( ui.button( { 28 + ( i % 2 ) * ( w + 16 ), top + ( i / 2 ) * ( buttonHeight + 14 ), w, buttonHeight }, names[i], i == 0 ) )
        {
            if ( i == 0 ) app.change( Screen::Playing ); if ( i == 1 ) app.save( true );
            if ( i == 2 ) { app.previous = Screen::Pause; app.change( Screen::Workshop ); }
            if ( i == 3 ) app.request( Action::Restart ); if ( i == 4 ) { app.save(); app.change( Screen::Campaign ); }
            if ( i == 5 ) { app.previous = Screen::Pause; app.change( Screen::Help ); }
        }
        const float settings = top + 3 * ( buttonHeight + 14 ) + 18;
        if ( ui.button( { 28, settings, w, buttonHeight }, TextFormat( "Text size / %d%%", static_cast<int>( app.scale * 100 + .5f ) ) ) ) app.scale = app.scale < 1.1f ? 1.15f : app.scale < 1.25f ? 1.3f : 1;
        if ( ui.button( { 44 + w, settings, w, buttonHeight }, app.muted ? "Sound / off" : "Sound / on" ) ) app.muted = !app.muted;
        if ( ui.button( { 28, bottom, w, buttonHeight }, "Title menu" ) ) { app.save(); app.change( Screen::Title ); }
        if ( ui.button( { l.width - w - 28, bottom, w, buttonHeight }, "Save and quit" ) ) { app.save(); app.quit = true; }
    }
    else if ( app.screen == Screen::Confirm )
    {
        menuHeading( ui, l, "CHECK YOUR MANIFEST", app.saveBlocked ? "Create a new save?" : app.confirmation == Action::EmptySandbox ? "Clear the sandbox floor?" : "Replace the current factory?" );
        Rectangle body = menuBody( l );
        ui.wrap( app.saveBlocked ? "The previous save could not be read. Starting this dispatch will preserve that file as campaign.sav.corrupt and create a new profile. Cancel keeps all files untouched." : app.confirmation == Action::EmptySandbox ? "This discards the sandbox layout and its cargo, then opens an empty floor paused. Your campaign progress remains unchanged." : "This discards the current factory, its cargo, and its dispatch timer. Your campaign medals and workshop upgrades remain. A restart restores the chapter's original budget and deposits.", body.x, body.y, body.width, ui.body(), Gold );
        if ( ui.button( { 28, bottom, 210 * app.scale, buttonHeight }, "Cancel [Esc]" ) ) app.change( app.previous );
        if ( ui.button( { l.width - 270 * app.scale - 28, bottom, 270 * app.scale, buttonHeight }, "Confirm [Enter]", true ) ) app.confirm();
    }
    else if ( app.screen == Screen::Result )
    {
        const bool won = app.sim.phase() == Phase::Won; const auto& m = missions()[app.mission];
        menuHeading( ui, l, won ? "FREIGHT CLEARED" : "DISPATCH MISSED", won ? m.title : "The line needs another plan" );
        Rectangle body = menuBody( l ); scrollInput( app, ui, body ); float y = body.y - app.scroll;
        y += paragraph( ui, won ? m.completion : app.sim.failure(), body.x, y, body.width, won ? Ink : Coral );
        const auto& stats = app.sim.stats();
        y += paragraph( ui, TextFormat( "Production %.1fs  /  Net construction %d credits  /  Scrap %u ore units", stats.elapsed, stats.spent - stats.refunds, stats.scrapped ), body.x, y, body.width, Gold );
        if ( won )
        {
            y += paragraph( ui, TextFormat( "%d / 3 stars. Campaign total: %d / 30.", app.sim.stars(), app.progress.totalStars() ), body.x, y, body.width, Mint );
            y += paragraph( ui, TextFormat( "Time star: at most %ds. Efficiency star: no scrap and net construction at most %d credits.", m.parTime, m.parCost ), body.x, y, body.width, Muted );
        }
        else y += paragraph( ui, "Your campaign progress is safe. Retry with a fresh budget and the same deposits. Plan before launch, then inspect buffers and dock requests when a route stops.", body.x, y, body.width, Muted );
        endScroll( app, ui, body, y );
        if ( ui.button( { 28, bottom, 210 * app.scale, buttonHeight }, "Campaign board" ) ) app.change( Screen::Campaign );
        if ( ui.button( { l.width - 300 * app.scale - 28, bottom, 300 * app.scale, buttonHeight }, won ? ( app.mission == 9 ? "Finish the story" : "Next dispatch" ) : "Retry dispatch", true ) )
        { if ( !won ) app.start( app.mission ); else if ( app.mission == 9 ) app.change( Screen::Ending ); else { app.selected = app.mission + 1; app.change( Screen::Briefing ); } }
    }
    else if ( app.screen == Screen::Ending )
    {
        menuHeading( ui, l, "LUMEN LINE / JOURNEY COMPLETE", "Homebound" ); Rectangle body = menuBody( l ); scrollInput( app, ui, body ); float y = body.y - app.scroll;
        y += paragraph( ui, missions()[9].completion, body.x, y, body.width );
        y += paragraph( ui, "At the coast, the passengers step onto a dry platform. The dog from the lift gets there first. Vale promises that the next factory will have better coffee. Iona asks for a window seat. For once, nobody asks you to build anything.", body.x, y, body.width, Mint );
        y += paragraph( ui, TextFormat( "All ten dispatches cleared. %d of 30 stars earned.", app.progress.totalStars() ), body.x, y, body.width, Gold );
        if ( app.progress.totalStars() >= 27 ) y += paragraph( ui, "You leave a clean set of plans for the engineers who will rebuild Lumen. In the margins, someone has drawn a little sun.", body.x, y, body.width );
        y += paragraph( ui, "VexFactory: The Last Freight\nBuilt with Vecs, raylib and GLFW. Art: Kenney Tiny Factory, CC0. Interface font: your system's font. Music is the noise you made along the way.", body.x, y, body.width, Muted );
        endScroll( app, ui, body, y ); if ( ui.button( { 28, bottom, 230 * app.scale, buttonHeight }, "Replay the campaign" ) ) app.change( Screen::Campaign );
        if ( ui.button( { l.width - 230 * app.scale - 28, bottom, 230 * app.scale, buttonHeight }, "Title menu", true ) ) app.change( Screen::Title );
    }
}
struct Options
{
    std::string assets = VEX_FACTORY_DEFAULT_ASSET_DIR, screenshot, font, scene;
    std::filesystem::path directory = defaultSaveDirectory();
    int frames = 0, mission = -1, width = 1440, height = 900;
    float scale = 0;
    bool smoke = false, noSave = false, noAudio = false, explicitAssets = false;
};
bool integer( const char* text, int& value, int minimum, int maximum )
{
    char* end = nullptr; long n = std::strtol( text, &end, 10 ); if ( !*text || *end || n < minimum || n > maximum ) return false; value = static_cast<int>( n ); return true;
}
int parse( int argc, char** argv, Options& o )
{
    for ( int i = 1; i < argc; ++i )
    {
        const std::string argument = argv[i];
        if ( argument == "--help" )
        {
            int major = 0, minor = 0, revision = 0; glfwGetVersion( &major, &minor, &revision );
            std::printf( "VexFactory: The Last Freight / raylib %s / GLFW %d.%d.%d\n", RAYLIB_VERSION, major, minor, revision );
            std::puts( "--assets DIR  --save-dir DIR  --font FILE.ttf  --ui-scale 1.0..1.4\n--no-save  --no-audio  --mission 1..10  --size WIDTH HEIGHT  --frames N\n--smoke-test  --screenshot FILE.png  --screen title|campaign|briefing|play|ending\nSpace: launch/pause; 1-9: parts; R: rotate; F: sorter filter; E: inspect; Tab: speed.\nWheel: zoom/scroll; middle mouse/WASD: pan; Home: fit; Esc: menu; F1: help.\nCtrl/Cmd+S: save; Ctrl/Cmd+Z: planning undo; Ctrl/Cmd +/-: text size; M: mute.\nFetch artwork: cmake -P demos/vexfactory/fetch_assets.cmake" ); return 1;
        }
        if ( argument == "--smoke-test" ) o.smoke = true;
        else if ( argument == "--no-save" ) o.noSave = true;
        else if ( argument == "--no-audio" ) o.noAudio = true;
        else if ( argument == "--assets" && i + 1 < argc ) { o.assets = argv[++i]; o.explicitAssets = true; }
        else if ( argument == "--save-dir" && i + 1 < argc ) { o.directory = argv[++i]; if ( o.directory.empty() ) return -1; }
        else if ( argument == "--font" && i + 1 < argc ) o.font = argv[++i];
        else if ( argument == "--screen" && i + 1 < argc ) o.scene = argv[++i];
        else if ( argument == "--screenshot" && i + 1 < argc ) { o.screenshot = argv[++i]; o.smoke = true; }
        else if ( argument == "--frames" && i + 1 < argc ) { if ( !integer( argv[++i], o.frames, 1, 1000000 ) ) return -1; }
        else if ( argument == "--mission" && i + 1 < argc ) { if ( !integer( argv[++i], o.mission, 1, 10 ) ) return -1; --o.mission; }
        else if ( argument == "--size" && i + 2 < argc ) { if ( !integer( argv[++i], o.width, 960, 7680 ) || !integer( argv[++i], o.height, 640, 4320 ) ) return -1; }
        else if ( argument == "--ui-scale" && i + 1 < argc ) { char* end = nullptr; o.scale = std::strtof( argv[++i], &end ); if ( *end || !std::isfinite( o.scale ) || o.scale < 1 || o.scale > 1.4f ) return -1; }
        else { std::fprintf( stderr, "Unknown or incomplete argument: %s\n", argv[i] ); return -1; }
    }
    if ( !o.scene.empty() && o.scene != "title" && o.scene != "campaign" && o.scene != "briefing" && o.scene != "play" && o.scene != "ending" ) return -1;
    if ( !o.scene.empty() && !o.smoke ) { std::fprintf( stderr, "--screen is a smoke/screenshot option.\n" ); return -1; }
    if ( o.smoke ) { o.noSave = o.noAudio = true; if ( !o.frames ) o.frames = 4; } return 0;
}
}
int main( int argc, char** argv )
{
    Options options; const int parsed = parse( argc, argv, options ); if ( parsed ) { if ( parsed < 0 ) std::fprintf( stderr, "Invalid arguments. Use --help.\n" ); return parsed < 0 ? 1 : 0; }
    if ( !options.explicitAssets )
    {
        const std::string portable = std::string( GetApplicationDirectory() ) + "assets";
        if ( FileExists( ( portable + "/Tilemap/tilemap_packed.png" ).c_str() ) ) options.assets = portable;
    }
    const std::string tilemap = options.assets + "/Tilemap/tilemap_packed.png";
    if ( !FileExists( tilemap.c_str() ) ) { std::fprintf( stderr, "VexFactory artwork is missing: %s\nRun: cmake -P demos/vexfactory/fetch_assets.cmake\nOr build the explicit vex_factory_assets target.\n", tilemap.c_str() ); return 1; }
    Image image = LoadImage( tilemap.c_str() );
    if ( !image.data || image.width != 192 || image.height != 176 ) { if ( image.data ) UnloadImage( image ); std::fprintf( stderr, "Fetch the pinned Tiny Factory tilemap (192 x 176).\n" ); return 1; }
    if ( !glfwInit() ) { const char* error = nullptr; glfwGetError( &error ); std::fprintf( stderr, "A desktop display is required: %s\n", error ? error : "GLFW initialization failed" ); UnloadImage( image ); return 1; }
    int count = 0; GLFWmonitor** monitors = glfwGetMonitors( &count );
    if ( !count ) { std::fprintf( stderr, "No desktop monitors found. Simulation tests need no display; Linux graphical tests can use Xvfb.\n" ); UnloadImage( image ); glfwTerminate(); return 1; }
    const GLFWvidmode* mode = glfwGetVideoMode( monitors[0] );
    if ( mode ) { options.width = std::min( options.width, std::max( 960, mode->width - 80 ) ); options.height = std::min( options.height, std::max( 640, mode->height - 100 ) ); }
    SetConfigFlags( FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT );
    InitWindow( options.width, options.height, "VexFactory: The Last Freight" );
    if ( !IsWindowReady() ) { UnloadImage( image ); return 1; }
    SetWindowMinSize( 960, 640 ); SetExitKey( KEY_NULL ); SetTargetFPS( 60 );
    Texture2D atlas = LoadTextureFromImage( image ); UnloadImage( image );
    if ( !IsTextureValid( atlas ) ) { CloseWindow(); return 1; } SetTextureFilter( atlas, TEXTURE_FILTER_POINT );
    FontFace font; font.choose( options.font );
    App app; app.noSave = options.noSave; app.saveDirectory = options.directory; app.load();
    if ( options.scale ) app.scale = options.scale;
    app.audio.initialize( options.noAudio || options.smoke ); app.saveClock = GetTime();
    if ( options.mission >= 0 && !options.smoke )
    {
        if ( options.mission <= app.progress.unlocked() ) { app.selected = options.mission; app.change( Screen::Briefing ); }
        else app.message( "That chapter is locked. Clear the preceding dispatches first." );
    }
    if ( options.smoke )
    {
        if ( options.mission >= 0 )
        {
            for ( int i = 0; i < options.mission; ++i ) app.progress.reward( i, 2, 60 );
            app.start( options.mission );
        }
        else { app.start( -1 ); for ( int i = 0; i < 60 * 35; ++i ) app.sim.step(); }
        if ( options.scene == "title" ) app.change( Screen::Title );
        if ( options.scene == "campaign" ) app.change( Screen::Campaign );
        if ( options.scene == "briefing" ) { app.selected = std::max( 0, options.mission ); app.change( Screen::Briefing ); }
        if ( options.scene == "ending" ) { for ( int i = 0; i < 10; ++i ) app.progress.reward( i, 3, 100 ); app.change( Screen::Ending ); }
    }
    int frames = 0; bool screenshotWritten = options.screenshot.empty();
    while ( !WindowShouldClose() && !app.quit && ( !options.frames || frames < options.frames ) )
    {
        font.update(); const float delta = std::min( GetFrameTime(), .15f );
        app.toastTime = std::max( 0.0f, app.toastTime - delta );
        Layout layout = Layout::make( static_cast<float>( GetScreenWidth() ), static_cast<float>( GetScreenHeight() ), app.scale ); fitCamera( app, layout );
        if ( !options.smoke )
        {
            globalInput( app );
            layout = Layout::make( static_cast<float>( GetScreenWidth() ), static_cast<float>( GetScreenHeight() ), app.scale );
            fitCamera( app, layout ); worldInput( app, layout );
        }
        if ( app.screen == Screen::Playing && !app.paused && !options.smoke )
        {
            app.accumulator += delta * app.speed;
            while ( app.accumulator >= FixedStep ) { app.sim.step(); app.accumulator -= FixedStep; }
            app.finish();
        }
        if ( GetTime() - app.saveClock > 20 ) app.save();
        BeginDrawing(); ClearBackground( Background );
        Ui ui{ font.font, app.scale, options.smoke ? Vector2{ -100, -100 } : GetMousePosition(), !options.smoke };
        if ( app.screen == Screen::Playing ) { drawFactory( app, layout, atlas ); drawHud( app, ui, layout, atlas ); }
        else drawMenus( app, ui, layout, atlas );
        if ( app.toastTime > 0 )
        {
            const float width = std::min( layout.width - 32, 760.0f * app.scale );
            const float height = ui.wrap( app.toast, 0, 0, width - 28, ui.body(), Ink, false ) + 24;
            Rectangle r{ 16, layout.height - layout.footer.height - height - 12, width, height }; ui.box( r, { 34, 56, 64, 250 } ); ui.wrap( app.toast, r.x + 14, r.y + 12, width - 28, ui.body(), Ink );
        }
        EndDrawing(); ++frames;
        if ( frames == 1 ) { std::puts( "GAME: Ready" ); std::fflush( stdout ); }
        if ( !options.screenshot.empty() && frames == options.frames ) { Image shot = LoadImageFromScreen(); screenshotWritten = ExportImage( shot, options.screenshot.c_str() ); UnloadImage( shot ); }
    }
    app.save(); app.audio.release(); font.release(); UnloadTexture( atlas ); CloseWindow();
    if ( !screenshotWritten ) { std::fprintf( stderr, "The screenshot was not written.\n" ); return 1; } return 0;
}
