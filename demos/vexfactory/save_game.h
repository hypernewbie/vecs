#pragma once
#include "campaign.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace vexfactory
{
struct SaveGame
{
    Progress progress;
    bool hasFactory = false, paused = true, muted = false;
    int mission = -1;
    float uiScale = 1;
    FactoryState factory;
};
inline std::filesystem::path defaultSaveDirectory()
{
#ifdef _WIN32
    wchar_t* home = nullptr; size_t length = 0;
    if ( _wdupenv_s( &home, &length, L"LOCALAPPDATA" ) == 0 && home )
    {
        const auto directory = std::filesystem::path( home ) / L"VexFactory";
        std::free( home ); return directory;
    }
    std::free( home );
#elif defined( __APPLE__ )
    if ( const char* home = std::getenv( "HOME" ) ) return std::filesystem::path( home ) / "Library/Application Support/VexFactory";
#else
    if ( const char* home = std::getenv( "XDG_DATA_HOME" ) ) return std::filesystem::path( home ) / "vexfactory";
    if ( const char* home = std::getenv( "HOME" ) ) return std::filesystem::path( home ) / ".local/share/vexfactory";
#endif
    return std::filesystem::path( "temp/vexfactory/profile" );
}
inline uint64_t saveChecksum( const std::string& bytes )
{
    uint64_t hash = 14695981039346656037ull;
    for ( unsigned char b : bytes ) { hash ^= b; hash *= 1099511628211ull; } return hash;
}
inline bool validProgress( const Progress& progress )
{
    int earned = 0, spent = 0; bool gap = false;
    for ( int i = 0; i < MissionCount; ++i )
    {
        const int stars = progress.stars[i]; const double time = progress.bestTime[i];
        if ( stars < 0 || stars > 3 || !std::isfinite( time ) || time < 0 || time > 1e8 || ( stars == 0 ) != ( time == 0 ) || ( gap && stars ) ) return false;
        gap |= stars == 0; if ( stars ) earned += 2 + stars;
    }
    constexpr int prices[] = { 2, 3, 5 };
    for ( int tier : progress.upgrades ) { if ( tier < 0 || tier > 3 ) return false; for ( int i = 0; i < tier; ++i ) spent += prices[i]; }
    return progress.tokens == earned - spent && progress.tokens >= 0;
}
inline bool validSave( const SaveGame& game )
{
    if ( !validProgress( game.progress ) || !std::isfinite( game.uiScale ) || game.uiScale < 1 || game.uiScale > 1.4f ) return false;
    if ( !game.hasFactory ) return true;
    if ( game.mission < -1 || game.mission >= MissionCount || game.mission > game.progress.unlocked() || game.factory.scenario.campaign != ( game.mission >= 0 ) ) return false;
    const auto& rules = game.factory.scenario;
    if ( game.mission >= 0 )
    {
        const std::array<int, 3> tiers = { rules.beltTier, rules.processingTier, rules.recoveryTier };
        for ( int i = 0; i < 3; ++i ) if ( tiers[i] > game.progress.upgrades[i] ) return false;
        const auto expected = scenarioFor( game.mission, tiers );
        if ( rules.credits != expected.credits || rules.power != expected.power || rules.unlocked != expected.unlocked || rules.deadline != expected.deadline || rules.parTime != expected.parTime || rules.parCost != expected.parCost || rules.goals != expected.goals || rules.ore != expected.ore || rules.blocked != expected.blocked || rules.docks.size() != expected.docks.size() ) return false;
        for ( size_t i = 0; i < rules.docks.size(); ++i ) if ( rules.docks[i].x != expected.docks[i].x || rules.docks[i].y != expected.docks[i].y || rules.docks[i].mask != expected.docks[i].mask ) return false;
        if ( game.factory.phase == Phase::Won && !game.progress.stars[game.mission] ) return false;
    }
    else if ( rules.beltTier || rules.processingTier || rules.recoveryTier ) return false;
    return Simulation::valid( game.factory );
}
inline std::string encodeSave( const SaveGame& game )
{
    std::ostringstream body; body.imbue( std::locale::classic() ); body << std::setprecision( std::numeric_limits<double>::max_digits10 );
    body << game.progress.tokens << ' ' << game.uiScale << ' ' << game.muted << '\n';
    for ( int i = 0; i < MissionCount; ++i ) body << game.progress.stars[i] << ' ' << game.progress.bestTime[i] << '\n';
    for ( int tier : game.progress.upgrades ) body << tier << ' '; body << '\n';
    body << game.hasFactory << ' ' << game.mission << ' ' << game.paused << '\n';
    if ( game.hasFactory )
    {
        const auto& s = game.factory;
        body << s.scenario.beltTier << ' ' << s.scenario.processingTier << ' ' << s.scenario.recoveryTier << ' ' << static_cast<int>( s.phase ) << '\n';
        const auto& t = s.stats;
        body << t.produced << ' ' << t.scrapped << ' ' << t.elapsed << ' ' << t.credits << ' ' << t.spent << ' ' << t.refunds << '\n';
        for ( auto n : t.delivered ) body << n << ' '; body << '\n';
        for ( auto n : s.remainingOre ) body << n << ' '; body << '\n';
        body << s.buildings.size() << '\n';
        for ( const auto& b : s.buildings )
            body << b.cell.x << ' ' << b.cell.y << ' ' << b.cell.direction << ' ' << static_cast<int>( b.building.tool ) << ' ' << b.building.locked << ' ' << b.building.paid << ' ' << b.cooldown << ' ' << b.progress << ' ' << b.stored << ' ' << b.plates << ' ' << b.dockMask << ' ' << b.working << ' ' << b.ready << ' ' << b.nextLeft << ' ' << static_cast<int>( b.filter ) << '\n';
        body << s.parcels.size() << '\n';
        for ( const auto& p : s.parcels ) body << static_cast<int>( p.kind ) << ' ' << p.transit.x << ' ' << p.transit.y << ' ' << p.transit.fromX << ' ' << p.transit.fromY << ' ' << p.transit.progress << '\n';
        body << s.shipments.size() << '\n'; for ( double time : s.shipments ) body << time << ' '; body << '\n';
    }
    const std::string payload = body.str(); std::ostringstream header;
    header << "VEX_FACTORY_SAVE 1 " << std::hex << saveChecksum( payload ) << '\n'; return header.str() + payload;
}
inline bool decodeSave( const std::string& bytes, SaveGame& destination, std::string& error )
{
    const auto newline = bytes.find( '\n' ); if ( newline == std::string::npos || bytes.size() > 2 * 1024 * 1024 ) { error = "The save is truncated or too large."; return false; }
    std::istringstream header( bytes.substr( 0, newline ) ); header.imbue( std::locale::classic() );
    std::string magic; int version = 0; uint64_t checksum = 0;
    if ( !( header >> magic >> version >> std::hex >> checksum ) || magic != "VEX_FACTORY_SAVE" || version != 1 ) { error = "The save format is not supported."; return false; }
    const std::string payload = bytes.substr( newline + 1 );
    if ( saveChecksum( payload ) != checksum ) { error = "The save checksum does not match."; return false; }
    SaveGame game; std::istringstream in( payload ); in.imbue( std::locale::classic() );
    in >> game.progress.tokens >> game.uiScale >> game.muted;
    for ( int i = 0; i < MissionCount; ++i ) in >> game.progress.stars[i] >> game.progress.bestTime[i];
    for ( int& tier : game.progress.upgrades ) in >> tier;
    in >> game.hasFactory >> game.mission >> game.paused;
    if ( game.hasFactory )
    {
        int phase = 0; std::array<int, 3> tiers{}; in >> tiers[0] >> tiers[1] >> tiers[2] >> phase;
        for ( int tier : tiers ) if ( tier < 0 || tier > 3 ) { error = "Invalid factory upgrade level."; return false; }
        if ( game.mission < -1 || game.mission >= MissionCount ) { error = "Invalid chapter number."; return false; }
        auto& s = game.factory; s.scenario = game.mission >= 0 ? scenarioFor( game.mission, tiers ) : Scenario{}; s.phase = static_cast<Phase>( phase );
        auto& t = s.stats; in >> t.produced >> t.scrapped >> t.elapsed >> t.credits >> t.spent >> t.refunds;
        for ( auto& n : t.delivered ) in >> n; for ( auto& n : s.remainingOre ) in >> n;
        size_t count = 0;
        if ( !( in >> count ) || count > Width * Height ) { error = "Invalid building count."; return false; }
        for ( size_t i = 0; i < count; ++i )
        {
            BuildingState b; int tool = 0, filter = 0;
            in >> b.cell.x >> b.cell.y >> b.cell.direction >> tool >> b.building.locked >> b.building.paid >> b.cooldown >> b.progress >> b.stored >> b.plates >> b.dockMask >> b.working >> b.ready >> b.nextLeft >> filter;
            b.building.tool = static_cast<Tool>( tool ); b.filter = static_cast<Item>( filter ); s.buildings.push_back( b );
        }
        if ( !( in >> count ) || count > Width * Height ) { error = "Invalid parcel count."; return false; }
        for ( size_t i = 0; i < count; ++i ) { ParcelState p{}; int kind = 0; in >> kind >> p.transit.x >> p.transit.y >> p.transit.fromX >> p.transit.fromY >> p.transit.progress; p.kind = static_cast<Item>( kind ); s.parcels.push_back( p ); }
        if ( !( in >> count ) || count > 40000 ) { error = "Invalid shipment count."; return false; }
        for ( size_t i = 0; i < count; ++i ) { double time = 0; in >> time; s.shipments.push_back( time ); }
        t.items = static_cast<uint32_t>( s.parcels.size() );
        t.perMinute = static_cast<float>( s.shipments.size() * 60.0 / std::min( 60.0, std::max( 1.0, t.elapsed ) ) );
    }
    if ( !in || !validSave( game ) ) { error = "The save contains invalid factory or campaign state."; return false; }
    in >> std::ws; if ( !in.eof() ) { error = "The save contains unexpected trailing data."; return false; }
    destination = std::move( game ); error.clear(); return true;
}
inline bool readSaveFile( const std::filesystem::path& path, SaveGame& game, std::string& error )
{
    std::error_code ec; const auto size = std::filesystem::file_size( path, ec );
    if ( ec || size > 2 * 1024 * 1024 ) { error = "Save file is missing or too large."; return false; }
    std::ifstream file( path, std::ios::binary ); std::ostringstream data; data << file.rdbuf();
    if ( !file || file.bad() ) { error = "Cannot read the save file."; return false; }
    return decodeSave( data.str(), game, error );
}
inline bool loadGame( const std::filesystem::path& directory, SaveGame& game, std::string& message )
{
    const auto path = directory / "campaign.sav";
    if ( readSaveFile( path, game, message ) ) return true;
    std::string backupError;
    if ( readSaveFile( directory / "campaign.sav.bak", game, backupError ) ) { message = "Recovered the previous save from backup."; return true; }
    return false;
}
inline bool saveGame( const std::filesystem::path& directory, const SaveGame& game, std::string& message )
{
    if ( !validSave( game ) ) { message = "Refused to save invalid game state."; return false; }
    std::error_code ec; std::filesystem::create_directories( directory, ec );
    if ( ec ) { message = "Cannot create the save directory: " + ec.message(); return false; }
    const auto path = directory / "campaign.sav", temporary = directory / "campaign.sav.tmp", backup = directory / "campaign.sav.bak";
    { std::ofstream out( temporary, std::ios::binary | std::ios::trunc ); const auto data = encodeSave( game ); out.write( data.data(), static_cast<std::streamsize>( data.size() ) ); out.flush(); if ( !out ) { message = "Cannot write the save file. Your previous save remains unchanged."; return false; } }
    if ( std::filesystem::exists( path, ec ) )
    {
        SaveGame previous; std::string ignored;
        const bool validPrevious = readSaveFile( path, previous, ignored );
        const auto old = validPrevious ? backup : directory / "campaign.sav.corrupt";
        std::filesystem::remove( old, ec ); ec.clear(); std::filesystem::rename( path, old, ec );
        if ( ec ) { message = "Cannot preserve the previous save: " + ec.message(); return false; }
    }
    std::filesystem::rename( temporary, path, ec );
    if ( ec )
    {
        message = "Cannot finish the save: " + ec.message(); std::error_code restoreError;
        if ( std::filesystem::exists( backup, restoreError ) ) std::filesystem::copy_file( backup, path, std::filesystem::copy_options::overwrite_existing, restoreError );
        return false;
    }
    message = "Game saved."; return true;
}
}
