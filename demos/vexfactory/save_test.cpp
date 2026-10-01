#define NOMINMAX
#include "save_game.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
using namespace vexfactory;
namespace
{
int checks = 0;
void check( bool value, const char* text, int line ) { ++checks; if ( !value ) { std::fprintf( stderr, "FAIL %d: %s\n", line, text ); std::exit( 1 ); } }
#define CHECK(...) check( (__VA_ARGS__), #__VA_ARGS__, __LINE__ )
void line( Simulation& sim )
{
    beginMission( sim, 0 );
    for ( int x = 2; x < 22; ++x ) CHECK( sim.place( x, 3, Tool::Belt, 0 ) );
    sim.launch(); for ( int i = 0; i < 500; ++i ) sim.step();
}
}
int main()
{
    Simulation original, resumed; line( original ); SaveGame game;
    game.hasFactory = true; game.mission = 0; game.factory = original.capture(); game.uiScale = 1.3f; game.muted = true;
    CHECK( validSave( game ) ); std::string bytes = encodeSave( game ), message; SaveGame loaded;
    CHECK( decodeSave( bytes, loaded, message ) ); CHECK( encodeSave( loaded ) == bytes ); CHECK( resumed.restore( loaded.factory ) );
    for ( int i = 0; i < 400; ++i ) { original.step(); resumed.step(); }
    CHECK( original.stats().delivered == resumed.stats().delivered ); CHECK( original.stats().elapsed == resumed.stats().elapsed );
    CHECK( original.stats().credits == resumed.stats().credits ); CHECK( original.oreLeft() == resumed.oreLeft() );
    CHECK( original.phase() == resumed.phase() );
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count(); std::error_code ec;
    auto directory = std::filesystem::temp_directory_path( ec ) / ( "vexfactory-save-test-" + std::to_string( unique ) ); CHECK( !ec );
    CHECK( saveGame( directory, game, message ) ); CHECK( loadGame( directory, loaded, message ) ); CHECK( encodeSave( loaded ) == bytes );
    SaveGame second = game; second.uiScale = 1.15f; CHECK( saveGame( directory, second, message ) );
    CHECK( loadGame( directory, loaded, message ) ); CHECK( loaded.uiScale == second.uiScale );
    { std::ofstream corrupt( directory / "campaign.sav" ); corrupt << "incomplete write"; }
    CHECK( loadGame( directory, loaded, message ) ); CHECK( message.find( "backup" ) != std::string::npos ); CHECK( loaded.uiScale == game.uiScale );
    CHECK( saveGame( directory, second, message ) ); CHECK( loadGame( directory, loaded, message ) ); CHECK( loaded.uiScale == second.uiScale );
    CHECK( std::filesystem::exists( directory / "campaign.sav.corrupt" ) );
    SaveGame largeText = game; largeText.uiScale = 2;
    CHECK( decodeSave( encodeSave( largeText ), loaded, message ) ); CHECK( loaded.uiScale == 2 );
    CHECK( decodeSave( bytes, loaded, message ) ); CHECK( loaded.uiScale == game.uiScale ); // Existing v1 profiles remain readable.
    const float before = loaded.uiScale;
    CHECK( !decodeSave( bytes.substr( 0, bytes.size() / 2 ), loaded, message ) ); CHECK( loaded.uiScale == before );
    std::string tampered = bytes; tampered.back() ^= 1; CHECK( !decodeSave( tampered, loaded, message ) );
    SaveGame bad = game; bad.factory.buildings.push_back( bad.factory.buildings.front() );
    CHECK( !decodeSave( encodeSave( bad ), loaded, message ) ); CHECK( !saveGame( directory, bad, message ) );
    CHECK( loadGame( directory, loaded, message ) ); CHECK( loaded.uiScale == second.uiScale );
    bad = game; bad.factory.parcels[0].transit.x = Width; CHECK( !decodeSave( encodeSave( bad ), loaded, message ) );
    bad = game; bad.factory.stats.credits = -1; CHECK( !decodeSave( encodeSave( bad ), loaded, message ) );
    bad = game; bad.factory.stats.credits++; CHECK( !decodeSave( encodeSave( bad ), loaded, message ) );
    bad = game; bad.factory.buildings[0].building.locked = false; CHECK( !decodeSave( encodeSave( bad ), loaded, message ) );
    bad = game; bad.factory.phase = Phase::Won; bad.progress.reward( 0, 3, 20 ); CHECK( !decodeSave( encodeSave( bad ), loaded, message ) );
    bad = game; bad.progress.tokens = 100; CHECK( !decodeSave( encodeSave( bad ), loaded, message ) );
    bad = game; bad.uiScale = 0.1f; CHECK( !decodeSave( encodeSave( bad ), loaded, message ) );
    bad = game; bad.uiScale = 2.1f; CHECK( !decodeSave( encodeSave( bad ), loaded, message ) );
    bad = game; bad.factory.stats.produced++; CHECK( !decodeSave( encodeSave( bad ), loaded, message ) );
    bad = game; bad.factory.buildings[0].cell.direction = 99; CHECK( !decodeSave( encodeSave( bad ), loaded, message ) );
    // Free tutorial parts cannot be demolished for cash or invalidate the save ledger.
    Simulation tutorial; beginMission( tutorial, 0 ); CHECK( tutorial.erase( 1, 3 ) ); CHECK( tutorial.stats().refunds == 0 );
    game.factory = tutorial.capture(); CHECK( validSave( game ) );
    Progress progress; progress.reward( 0, 3, 20 ); CHECK( progress.purchase( 0 ) ); CHECK( validProgress( progress ) );
    progress.reward( 0, 3, 18 ); CHECK( validProgress( progress ) );
    std::filesystem::remove_all( directory, ec ); CHECK( !ec );
    std::printf( "VexFactory persistence: roundtrip, continuation, atomic backup and invalid-save tests passed (%d checks).\n", checks );
}
