#define NOMINMAX
#include "campaign.h"
#include <cstdio>
#include <cstdlib>

using namespace vexfactory;
namespace
{
int checks = 0;
void require( bool b, const char* expr, int line ) { ++checks; if ( !b ) { std::fprintf( stderr, "FAIL %d: %s\n", line, expr ); std::exit( 1 ); } }
#define CHECK(...) require( (__VA_ARGS__), #__VA_ARGS__, __LINE__ )
void put( Simulation& sim, int x, int y, Tool tool, int dir = 0 )
{
    std::string why;
    if ( !sim.canPlace( x, y, tool, &why ) ) std::fprintf( stderr, "Cannot place %s at %d,%d: %s (credits %d, power %d/%d)\n", toolName( tool ), x, y, why.c_str(), sim.stats().credits, sim.stats().powerUsed, sim.stats().powerLimit );
    CHECK( sim.place( x, y, tool, dir ) );
}
void lane( Simulation& sim, int y )
{
    put( sim, 1, y, Tool::Miner );
    for ( int x = 2; x < 22; ++x ) if ( !sim.blocked( x, y ) ) put( sim, x, y, Tool::Belt );
}
// Test-only witnesses, not an auto-build feature. All purchases obey game rules.
void solution( Simulation& sim, int id )
{
    beginMission( sim, id );
    if ( id < 3 )
    {
        const int y = id == 2 ? 13 : 3; lane( sim, y );
        if ( id >= 1 ) put( sim, 5, y, Tool::Smelter );
        if ( id == 2 ) put( sim, 10, y, Tool::Press );
    }
    else if ( id == 3 )
    {
        lane( sim, 8 ); put( sim, 5, 8, Tool::Smelter ); put( sim, 10, 8, Tool::Splitter );
        put( sim, 10, 7, Tool::Belt, 3 ); put( sim, 10, 6, Tool::Press, 3 );
        put( sim, 10, 5, Tool::Belt, 3 ); put( sim, 10, 4, Tool::Belt, 3 );
        for ( int x = 10; x < 22; ++x ) put( sim, x, 3, Tool::Belt );
    }
    else if ( id == 4 )
    {
        put( sim, 1, 8, Tool::Miner );
        for ( int x = 2; x <= 18; ++x ) put( sim, x, 8, Tool::Belt );
        put( sim, 5, 8, Tool::Smelter ); put( sim, 10, 8, Tool::Splitter ); put( sim, 13, 8, Tool::Press );
        put( sim, 18, 8, Tool::Belt, 3 );
        for ( int y = 4; y < 8; ++y ) { put( sim, 10, y, Tool::Belt, 3 ); put( sim, 18, y, Tool::Belt, 3 ); }
        for ( int x = 10; x < 22; ++x ) put( sim, x, 3, Tool::Belt );
        put( sim, 18, 3, Tool::Assembler );
    }
    else
    {
        const int generators = ( 28 - sim.scenario().power + 11 ) / 12;
        for ( int i = 0; i < generators; ++i ) put( sim, 20 + i, 0, Tool::Generator );
        for ( int y : { 3, 8, 13 } ) { lane( sim, y ); put( sim, 5, y, Tool::Smelter ); }
        put( sim, 10, 3, Tool::Press ); put( sim, 15, 3, Tool::Assembler ); put( sim, 10, 13, Tool::Press );
        put( sim, 15, 8, Tool::Splitter );
        for ( int y = 4; y < 8; ++y ) put( sim, 15, y, Tool::Belt, 3 );
        if ( sim.blocked( 12, 13 ) )
        {
            put( sim, 11, 13, Tool::Belt, 3 ); put( sim, 11, 12, Tool::Belt ); put( sim, 12, 12, Tool::Belt );
            put( sim, 13, 12, Tool::Belt, 1 );
        }
        if ( sim.blocked( 12, 3 ) )
        {
            put( sim, 11, 3, Tool::Belt, 3 ); put( sim, 11, 2, Tool::Belt ); put( sim, 12, 2, Tool::Belt );
            put( sim, 13, 2, Tool::Belt, 1 );
        }
    }
}
void conserved( Simulation& sim )
{
    uint64_t total = sim.stats().scrapped + sim.materialInFactory();
    for ( int i = 0; i < ItemCount; ++i ) total += static_cast<uint64_t>( sim.stats().delivered[i] ) * sim.weight( static_cast<Item>( i ) );
    CHECK( total == sim.stats().produced );
    CHECK( sim.stats().credits >= 0 ); CHECK( sim.stats().powerUsed <= sim.stats().powerLimit );
    CHECK( Simulation::valid( sim.capture() ) );
}
void allMissions()
{
    Progress progress;
    for ( int id = 0; id < MissionCount; ++id )
    {
        Simulation sim; solution( sim, id );
        CHECK( sim.phase() == Phase::Planning );
        for ( int i = 0; i < 300; ++i ) sim.step();
        CHECK( sim.stats().elapsed == 0 ); CHECK( sim.stats().produced == 0 );
        sim.launch();
        while ( sim.phase() == Phase::Running ) { sim.step(); if ( static_cast<int>( sim.stats().elapsed * 60 ) % 600 == 0 ) conserved( sim ); }
        std::printf( "Chapter %02d: %s / %.1fs / %d stars / net cost %d / %s\n", id + 1, missions()[id].title, sim.stats().elapsed, sim.stars(), sim.stats().spent - sim.stats().refunds, sim.failure().c_str() );
        CHECK( sim.phase() == Phase::Won ); CHECK( sim.stars() >= 1 ); conserved( sim );
        CHECK( progress.reward( id, sim.stars(), sim.stats().elapsed ) >= 3 );
    }
    CHECK( progress.complete() ); CHECK( progress.unlocked() == 9 );
}
void economyAndPower()
{
    Scenario rules = scenarioFor( 0 ); rules.credits = 65; Simulation sim; sim.configure( rules );
    const int original = sim.stats().credits;
    CHECK( !sim.place( 2, 3, Tool::Miner, 0 ) ); CHECK( sim.stats().credits == original );
    CHECK( !sim.erase( 22, 3 ) ); CHECK( !sim.place( 12, 1, Tool::Belt, 0 ) );
    put( sim, 1, 3, Tool::Miner ); CHECK( sim.stats().credits == 0 );
    CHECK( !sim.place( 2, 3, Tool::Smelter, 0 ) );
    CHECK( sim.erase( 1, 3 ) ); CHECK( sim.stats().credits == 45 );
    CHECK( !sim.place( 1, 3, Tool::Miner, 0 ) );
    rules = scenarioFor( 8 ); sim.configure( rules ); put( sim, 0, 0, Tool::Generator );
    put( sim, 1, 3, Tool::Miner ); put( sim, 2, 3, Tool::Smelter ); put( sim, 3, 3, Tool::Press );
    CHECK( !sim.erase( 0, 0 ) ); CHECK( !sim.place( 0, 0, Tool::Belt, 0 ) );
    CHECK( sim.erase( 1, 3 ) ); CHECK( sim.erase( 2, 3 ) ); CHECK( sim.erase( 3, 3 ) ); CHECK( sim.erase( 0, 0 ) );
}
void failuresAndDeposits()
{
    Simulation sim; beginMission( sim, 0 ); sim.launch();
    for ( int i = 0; i < 60 * 100; ++i ) sim.step();
    CHECK( sim.phase() == Phase::Lost ); CHECK( !sim.failure().empty() );
    Scenario rules = scenarioFor( 0 ); rules.goals[0] = 40; sim.configure( rules ); sim.launch(); sim.step();
    CHECK( sim.phase() == Phase::Lost ); CHECK( sim.failure().find( "ore" ) != std::string::npos );
    rules.goals[0] = 2; rules.ore.fill( 0 ); rules.ore[Simulation::index( 1, 3 )] = 2; sim.configure( rules );
    put( sim, 1, 3, Tool::Miner ); put( sim, 2, 3, Tool::Belt );
    sim.launch(); for ( int i = 0; i < 300; ++i ) sim.step(); CHECK( sim.stats().produced == 1 ); // One blocked slot.
    CHECK( sim.erase( 2, 3 ) ); sim.step(); CHECK( sim.phase() == Phase::Lost );
}
void snapshotContinuation()
{
    Simulation a, b; solution( a, 6 ); a.launch(); for ( int i = 0; i < 2700; ++i ) a.step();
    FactoryState state = a.capture(); CHECK( b.restore( state ) );
    for ( int i = 0; i < 2400; ++i ) { a.step(); b.step(); }
    CHECK( a.stats().delivered == b.stats().delivered ); CHECK( a.stats().produced == b.stats().produced );
    CHECK( a.stats().elapsed == b.stats().elapsed ); CHECK( a.stats().credits == b.stats().credits ); CHECK( a.oreLeft() == b.oreLeft() );
    CHECK( a.materialInFactory() == b.materialInFactory() ); conserved( a ); conserved( b );
    state.parcels.push_back( { Item::Ore, { -1, 0, 0, 0, 0 } } ); const auto before = b.stats().produced;
    CHECK( !b.restore( state ) ); CHECK( b.stats().produced == before );
}
void sorterAndRewards()
{
    Simulation sim; sim.reset( false );
    put( sim, 0, 3, Tool::Miner ); put( sim, 1, 3, Tool::Belt ); put( sim, 2, 3, Tool::Smelter );
    put( sim, 3, 3, Tool::Belt ); put( sim, 4, 3, Tool::Sorter ); put( sim, 5, 3, Tool::Belt ); put( sim, 6, 3, Tool::Shipping );
    put( sim, 4, 2, Tool::Belt, 3 ); put( sim, 4, 1, Tool::Shipping );
    for ( int i = 0; i < 1200; ++i ) sim.step(); CHECK( sim.stats().delivered[1] > 10 );
    sim.cycleFilter( 4, 3 ); bool branch = false;
    for ( int i = 0; i < 600; ++i ) { sim.step(); sim.parcels( [&]( vecsEntity, Parcel&, Transit& t ) { branch |= t.x == 4 && t.y == 2; } ); }
    CHECK( branch ); CHECK( Simulation::valid( sim.capture() ) );
    Progress p; CHECK( p.reward( 0, 1, 25 ) == 3 ); CHECK( p.reward( 0, 1, 26 ) == 0 ); CHECK( p.reward( 0, 3, 20 ) == 2 );
    CHECK( p.tokens == 5 ); CHECK( p.purchase( 0 ) ); CHECK( p.purchase( 0 ) ); CHECK( !p.purchase( 0 ) ); CHECK( p.upgrades[0] == 2 );
}
}
int main()
{
    allMissions(); economyAndPower(); failuresAndDeposits(); snapshotContinuation(); sorterAndRewards();
    std::printf( "VexFactory campaign: all ten chapters and systems passed (%d checks).\n", checks );
}
