#define NOMINMAX
#include "simulation.h"
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace vexfactory;

namespace
{
int checks = 0;
void require( bool condition, const char* expression, int line )
{
    ++checks;
    if ( !condition ) { std::fprintf( stderr, "FAIL line %d: %s\n", line, expression ); std::exit( 1 ); }
}
#define REQUIRE( ... ) require( ( __VA_ARGS__ ), #__VA_ARGS__, __LINE__ )

uint32_t shipped( const Simulation& sim )
{
    uint32_t result = 0;
    for ( uint32_t n : sim.stats().delivered ) result += n;
    return result;
}
void invariant( Simulation& sim )
{
    REQUIRE( sim.stats().produced == sim.materialInFactory() + sim.stats().scrapped + shipped( sim ) );
    std::array<bool, Width * Height> occupied{};
    uint32_t items = 0;
    sim.parcels( [&]( vecsEntity, Parcel& p, Transit& t )
    {
        REQUIRE( Simulation::inside( t.x, t.y ) );
        REQUIRE( !occupied[Simulation::index( t.x, t.y )] );
        occupied[Simulation::index( t.x, t.y )] = true;
        REQUIRE( sim.at( t.x, t.y ) != VECS_INVALID_ENTITY );
        REQUIRE( vecsHas<Conveyor>( sim.world(), sim.at( t.x, t.y ) ) );
        REQUIRE( t.progress >= 0 && t.progress <= 1 );
        REQUIRE( std::abs( t.x - t.fromX ) + std::abs( t.y - t.fromY ) == 1 );
        REQUIRE( static_cast<int>( p.kind ) >= 0 && static_cast<int>( p.kind ) < ItemCount );
        ++items;
    } );
    REQUIRE( items == sim.stats().items );
    uint32_t buildings = 0, working = 0;
    sim.buildings( [&]( vecsEntity e, Cell& cell, Building& )
    {
        REQUIRE( sim.at( cell.x, cell.y ) == e );
        REQUIRE( cell.direction >= 0 && cell.direction < 4 );
        if ( const Processor* p = vecsGet<Processor>( sim.world(), e ) )
        {
            REQUIRE( !( p->working && p->ready ) );
            REQUIRE( vecsGet<Inventory>( sim.world(), e )->stored <= BufferCapacity );
            REQUIRE( vecsHas<Working>( sim.world(), e ) == p->working );
            if ( p->working ) ++working;
        }
        ++buildings;
    } );
    REQUIRE( sim.entities() == items + buildings );
    REQUIRE( sim.stats().machinesWorking == working );
}
void run( Simulation& sim, int ticks )
{
    for ( int i = 0; i < ticks; ++i ) { sim.step(); if ( i % 60 == 0 ) invariant( sim ); }
    invariant( sim );
}
void belt( Simulation& sim, int from, int to, int y )
{
    for ( int x = from; x <= to; ++x ) sim.place( x, y, Tool::Belt, 0 );
}
void simpleLine( Simulation& sim )
{
    sim.reset( false );
    sim.place( 0, 0, Tool::Miner, 0 );
    belt( sim, 1, 3, 0 );
    sim.place( 4, 0, Tool::Shipping, 0 );
}

void emptyAndBounds()
{
    Simulation sim; sim.reset( false );
    REQUIRE( !sim.place( -1, 0, Tool::Miner, 0 ) );
    REQUIRE( !sim.place( Width, 0, Tool::Miner, 0 ) );
    REQUIRE( !sim.erase( 0, Height ) );
    REQUIRE( sim.at( 1, -1 ) == VECS_INVALID_ENTITY );
    run( sim, 180 );
    REQUIRE( sim.entities() == 0 );
    REQUIRE( sim.stats().produced == 0 );
}
void rawShippingAndReset()
{
    Simulation sim; simpleLine( sim );
    run( sim, 60 * 20 );
    REQUIRE( sim.stats().delivered[0] > 20 );
    REQUIRE( sim.stats().perMinute > 0 );
    sim.reset( false );
    REQUIRE( sim.stats().elapsed == 0 );
    REQUIRE( sim.stats().produced == 0 );
    REQUIRE( sim.stats().perMinute == 0 );
    REQUIRE( sim.entities() == 0 );
    run( sim, 120 );
    sim.reset( true );
    run( sim, 60 * 60 );
    REQUIRE( sim.stats().delivered[1] >= 20 );
    REQUIRE( sim.stats().delivered[2] >= 15 );
    REQUIRE( sim.stats().delivered[3] >= 10 );
}
void threeRecipes()
{
    Simulation sim; sim.reset( false );
    sim.place( 0, 1, Tool::Miner, 0 );
    belt( sim, 1, 12, 1 );
    sim.place( 3, 1, Tool::Smelter, 0 );
    sim.place( 6, 1, Tool::Press, 0 );
    sim.place( 9, 1, Tool::Assembler, 0 );
    sim.place( 13, 1, Tool::Shipping, 0 );
    run( sim, 60 * 35 );
    REQUIRE( sim.stats().delivered[3] > 8 );
    REQUIRE( sim.stats().delivered[0] == 0 );
    REQUIRE( sim.stats().delivered[1] == 0 );
    REQUIRE( sim.stats().delivered[2] == 0 );
}
void jamsAndWrongInput()
{
    Simulation sim; sim.reset( false );
    sim.place( 0, 0, Tool::Miner, 0 );
    belt( sim, 1, 3, 0 );
    run( sim, 60 * 10 );
    REQUIRE( sim.stats().produced == 3 );
    REQUIRE( sim.stats().items == 3 );
    REQUIRE( sim.stats().waiting == 3 );
    sim.place( 4, 0, Tool::Press, 0 ); // Ore is not a press input.
    run( sim, 600 );
    REQUIRE( vecsGet<Inventory>( sim.world(), sim.at( 4, 0 ) )->stored == 0 );
    REQUIRE( sim.stats().produced == 3 );
    sim.place( 4, 0, Tool::Smelter, 0 ); // Valid recipe, but no outgoing belt.
    run( sim, 60 * 20 );
    REQUIRE( vecsGet<Processor>( sim.world(), sim.at( 4, 0 ) )->ready );
    REQUIRE( vecsGet<Inventory>( sim.world(), sim.at( 4, 0 ) )->stored == BufferCapacity );
    const uint32_t produced = sim.stats().produced;
    run( sim, 600 );
    REQUIRE( sim.stats().produced == produced );
    REQUIRE( sim.materialInFactory() == 8 ); // Three belts + four inputs + one output.
    sim.erase( 4, 0 );
    REQUIRE( sim.stats().scrapped == 5 );
    invariant( sim );
}
void orientationAndTurns()
{
    Simulation sim; sim.reset( false );
    sim.place( 3, 1, Tool::Miner, 1 );
    sim.place( 3, 2, Tool::Belt, 1 );
    sim.place( 3, 3, Tool::Belt, 2 );
    sim.place( 2, 3, Tool::Belt, 3 );
    sim.place( 2, 2, Tool::Belt, 2 );
    sim.place( 1, 2, Tool::Shipping, 0 );
    run( sim, 600 );
    REQUIRE( sim.stats().delivered[0] > 5 );
    const vecsEntity same = sim.at( 3, 2 );
    sim.place( 3, 2, Tool::Belt, -1 );
    REQUIRE( sim.at( 3, 2 ) == same );
    REQUIRE( vecsGet<Cell>( sim.world(), same )->direction == 3 );
    sim.place( 3, 2, Tool::Belt, 5 );
    REQUIRE( vecsGet<Cell>( sim.world(), same )->direction == 1 );
    invariant( sim );
    sim.reset( false );
    sim.place( Width - 1, 0, Tool::Miner, 0 );
    run( sim, 600 );
    REQUIRE( sim.stats().produced == 0 );
}
void sharedOutputReservation()
{
    Simulation sim; sim.reset( false );
    sim.place( 0, 1, Tool::Miner, 0 );
    sim.place( 2, 1, Tool::Miner, 2 );
    sim.place( 1, 1, Tool::Belt, 1 );
    sim.place( 1, 2, Tool::Belt, 1 );
    sim.place( 1, 3, Tool::Shipping, 0 );
    sim.step();
    REQUIRE( sim.stats().produced == 1 );
    REQUIRE( sim.stats().items == 1 );
    run( sim, 60 * 15 );
    REQUIRE( sim.stats().delivered[0] > 15 );
}
void deletionAndRotation()
{
    Simulation sim; simpleLine( sim );
    sim.step();
    REQUIRE( sim.stats().items == 1 );
    REQUIRE( sim.erase( 1, 0 ) );
    REQUIRE( sim.stats().scrapped == 1 );
    REQUIRE( sim.stats().items == 0 );
    run( sim, 600 );
    REQUIRE( sim.stats().produced == 1 );
    REQUIRE( !sim.erase( 1, 0 ) );
    sim.place( 1, 0, Tool::Belt, 0 );
    sim.place( 2, 0, Tool::Smelter, 0 );
    for ( int i = 0; i < 600 && !vecsGet<Processor>( sim.world(), sim.at( 2, 0 ) )->working; ++i ) sim.step();
    const vecsEntity machine = sim.at( 2, 0 );
    REQUIRE( vecsGet<Processor>( sim.world(), machine )->working );
    const float progress = vecsGet<Processor>( sim.world(), machine )->progress;
    sim.place( 2, 0, Tool::Smelter, 1 );
    REQUIRE( sim.at( 2, 0 ) == machine );
    REQUIRE( vecsGet<Processor>( sim.world(), machine )->progress == progress );
    invariant( sim );
    sim.place( 2, 0, Tool::Shipping, 0 );
    REQUIRE( sim.stats().scrapped >= 2 );
    invariant( sim );
}
void rollingThroughput()
{
    Simulation sim; simpleLine( sim );
    run( sim, 60 * 10 );
    REQUIRE( sim.stats().perMinute > 0 );
    sim.erase( 0, 0 );
    run( sim, 60 * 70 );
    REQUIRE( sim.stats().perMinute == 0 );
}
void editStressAndDeterminism()
{
    Simulation a, b;
    std::mt19937 random( 0x564558 );
    for ( int i = 0; i < 4000; ++i )
    {
        const int x = static_cast<int>( random() % Width );
        const int y = static_cast<int>( random() % Height );
        const Tool tool = static_cast<Tool>( random() % 7 );
        const int direction = static_cast<int>( random() % 4 );
        a.place( x, y, tool, direction );
        b.place( x, y, tool, direction );
        for ( int step = 0; step < 3; ++step ) { a.step(); b.step(); }
        if ( i % 20 == 0 ) { invariant( a ); invariant( b ); }
        REQUIRE( a.stats().produced == b.stats().produced );
        REQUIRE( a.stats().delivered == b.stats().delivered );
        REQUIRE( a.stats().scrapped == b.stats().scrapped );
        REQUIRE( a.materialInFactory() == b.materialInFactory() );
    }
    invariant( a ); invariant( b );
}
}

int main()
{
    emptyAndBounds();
    rawShippingAndReset();
    threeRecipes();
    jamsAndWrongInput();
    orientationAndTurns();
    sharedOutputReservation();
    deletionAndRotation();
    rollingThroughput();
    editStressAndDeterminism();
    std::printf( "VexFactory: 9 simulation tests passed (%d checks).\n", checks );
    return 0;
}
