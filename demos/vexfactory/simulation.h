#pragma once

#include "vecs.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace vexfactory
{
constexpr int Width = 24, Height = 16;
constexpr float FixedStep = 1.0f / 60.0f;
constexpr uint32_t BufferCapacity = 4;
constexpr int DX[] = { 1, 0, -1, 0 }, DY[] = { 0, 1, 0, -1 };
enum class Tool { Belt, Miner, Smelter, Press, Assembler, Shipping, Erase, Splitter, Sorter, Generator };
enum class Item { Ore, Plate, Gear, Engine, Count };
enum class Phase { Sandbox, Planning, Running, Won, Lost };
constexpr int ItemCount = static_cast<int>( Item::Count ), ToolCount = 10;
inline bool isTransport( Tool t ) { return t == Tool::Belt || t == Tool::Splitter || t == Tool::Sorter; }
inline bool isProcessor( Tool t ) { return t == Tool::Smelter || t == Tool::Press || t == Tool::Assembler; }
inline const char* toolName( Tool t )
{
    constexpr const char* names[] = { "Conveyor", "Ore miner", "Smelter", "Gear press", "Assembler", "Freight dock", "Demolish", "Splitter", "Sorter", "Generator" };
    return names[static_cast<int>( t )];
}
inline const char* itemName( Item i )
{
    constexpr const char* names[] = { "Ore", "Plates", "Gears", "Engines" };
    return names[static_cast<int>( i )];
}
inline Item inputFor( Tool t ) { return t == Tool::Smelter ? Item::Ore : t == Tool::Press ? Item::Plate : Item::Gear; }
inline Item outputFor( Tool t ) { return static_cast<Item>( static_cast<int>( inputFor( t ) ) + 1 ); }
inline float durationFor( Tool t ) { return t == Tool::Smelter ? 1.1f : t == Tool::Press ? 1.5f : 2.0f; }
inline int costFor( Tool t ) { constexpr int costs[] = { 6, 65, 90, 125, 170, 0, 0, 50, 65, 130 }; return costs[static_cast<int>( t )]; }
inline int priceFor( Item i ) { constexpr int prices[] = { 1, 4, 10, 28 }; return prices[static_cast<int>( i )]; }
inline int powerFor( Tool t ) { return t == Tool::Miner ? 2 : t == Tool::Smelter ? 3 : t == Tool::Press ? 4 : t == Tool::Assembler ? 5 : 0; }
inline uint32_t toolBit( Tool t ) { return 1u << static_cast<int>( t ); }
inline uint32_t itemBit( Item i ) { return 1u << static_cast<int>( i ); }

struct Cell { int x, y, direction; };
struct Building { Tool tool; bool locked = false; int paid = 0; };
struct Conveyor { float speed = 3.0f; };
struct Producer { float cooldown = 0.0f; };
struct Inventory { uint32_t stored = 0, plates = 0; };
struct Processor { Tool recipe; float progress = 0.0f; bool working = false, ready = false; };
struct Shipping {};
struct Working {};
struct Dock { uint32_t mask = 15; };
struct Router { bool splitter = true, nextLeft = false; Item filter = Item::Plate; };
struct Parcel { Item kind; };
struct Transit { int x, y, fromX, fromY; float progress; };
struct DockSpec { int x, y; uint32_t mask; };
struct Scenario
{
    bool campaign = false;
    int credits = 0, power = 0;
    uint32_t unlocked = 0x3ff;
    double deadline = 300, parTime = 150;
    int parCost = 1000;
    int beltTier = 0, processingTier = 0, recoveryTier = 0;
    std::array<uint32_t, ItemCount> goals{};
    std::array<bool, Width * Height> blocked{};
    std::array<uint32_t, Width * Height> ore{};
    std::vector<DockSpec> docks;
};
struct Stats
{
    uint32_t produced = 0, scrapped = 0;
    std::array<uint32_t, ItemCount> delivered{};
    uint32_t items = 0, waiting = 0, machinesWorking = 0;
    double elapsed = 0;
    float perMinute = 0;
    int credits = 0, spent = 0, refunds = 0, powerUsed = 0, powerLimit = 0;
};
struct BuildingState
{
    Cell cell{};
    Building building{};
    float cooldown = 0, progress = 0;
    uint32_t stored = 0, plates = 0, dockMask = 15;
    bool working = false, ready = false, nextLeft = false;
    Item filter = Item::Plate;
};
struct ParcelState { Item kind; Transit transit; };
struct FactoryState
{
    Scenario scenario;
    Stats stats;
    Phase phase = Phase::Sandbox;
    std::array<uint32_t, Width * Height> remainingOre{};
    std::vector<BuildingState> buildings;
    std::vector<ParcelState> parcels;
    std::vector<double> shipments;
};

class Simulation
{
public:
    Simulation() { reset( true ); }
    ~Simulation() { release(); }
    Simulation( const Simulation& ) = delete;
    Simulation& operator=( const Simulation& ) = delete;
    static bool inside( int x, int y ) { return x >= 0 && y >= 0 && x < Width && y < Height; }
    static int index( int x, int y ) { return y * Width + x; }
    vecsWorld* world() const { return m_world; }
    const Stats& stats() const { return m_stats; }
    const Scenario& scenario() const { return m_rules; }
    Phase phase() const { return m_phase; }
    const std::string& failure() const { return m_failure; }
    vecsEntity at( int x, int y ) const { return inside( x, y ) ? m_grid[index( x, y )] : VECS_INVALID_ENTITY; }
    uint32_t entities() const { return vecsCount( m_world ); }
    uint32_t oreAt( int x, int y ) const { return inside( x, y ) ? m_ore[index( x, y )] : 0; }
    uint32_t oreLeft() const { uint32_t n = 0; for ( auto ore : m_ore ) n += ore; return n; }
    bool blocked( int x, int y ) const { return !inside( x, y ) || m_rules.blocked[index( x, y )]; }
    uint32_t weight( Item i ) const { constexpr uint32_t weights[] = { 1, 1, 2, 5 }; return m_rules.campaign ? weights[static_cast<int>( i )] : 1; }
    int refundFor( Tool t ) const { return costFor( t ) * ( 70 + 5 * m_rules.recoveryTier ) / 100; }
    int refundFor( const Building& b ) const { return b.paid * ( 70 + 5 * m_rules.recoveryTier ) / 100; }
    float workDuration( Tool t ) const { return durationFor( t ) / ( 1 + 0.15f * m_rules.processingTier ); }
    template<typename Fn> void buildings( Fn&& fn ) { vecsQueryEach<Cell, Building>( m_world, m_buildings, std::forward<Fn>( fn ) ); }
    template<typename Fn> void parcels( Fn&& fn ) { vecsQueryEach<Parcel, Transit>( m_world, m_parcels, std::forward<Fn>( fn ) ); }
    void reset( bool starter )
    {
        release(); m_world = vecsCreateWorld( 4096 );
        m_buildings = vecsBuildQuery<Cell, Building>( m_world );
        m_parcels = vecsBuildQuery<Parcel, Transit>( m_world );
        m_producers = vecsBuildQuery<Cell, Producer>( m_world );
        m_processors = vecsBuildQuery<Cell, Processor, Inventory>( m_world );
        m_working = vecsBuildQuery<Working>( m_world );
        m_grid.fill( VECS_INVALID_ENTITY ); m_ore.fill( 0 ); m_rules = {}; m_stats = {};
        m_phase = Phase::Sandbox; m_failure.clear(); m_shipments.clear();
        m_births.clear(); m_deaths.clear(); m_states.clear();
        if ( starter ) for ( int lane = 0; lane < 3; ++lane )
        {
            const int y = 3 + lane * 5;
            place( 1, y, Tool::Miner, 0 );
            for ( int x = 2; x <= 20; ++x ) place( x, y, Tool::Belt, 0 );
            place( 5, y, Tool::Smelter, 0 );
            if ( lane >= 1 ) place( 10, y, Tool::Press, 0 );
            if ( lane >= 2 ) place( 15, y, Tool::Assembler, 0 );
            place( 21, y, Tool::Shipping, 0 );
        }
    }
    void configure( const Scenario& scenario )
    {
        reset( false ); m_rules = scenario; m_ore = scenario.ore;
        m_phase = scenario.campaign ? Phase::Planning : Phase::Sandbox;
        m_initializing = true;
        for ( const auto& dock : scenario.docks )
        {
            place( dock.x, dock.y, Tool::Shipping, 0 );
            vecsSet<Dock>( m_world, at( dock.x, dock.y ), { dock.mask } );
            vecsGet<Building>( m_world, at( dock.x, dock.y ) )->locked = true;
        }
        m_initializing = false;
        m_stats.credits = scenario.credits; m_stats.spent = m_stats.refunds = 0; refreshCounts();
    }
    void seed( int x, int y, Tool t, int direction )
    {
        m_initializing = true; place( x, y, t, direction ); m_initializing = false;
    }
    void launch() { if ( m_phase == Phase::Planning ) m_phase = Phase::Running; }
    bool canPlace( int x, int y, Tool tool, std::string* reason = nullptr ) const
    {
        const auto reject = [&]( const char* s ) { if ( reason ) *reason = s; return false; };
        if ( !inside( x, y ) || static_cast<int>( tool ) < 0 || static_cast<int>( tool ) >= ToolCount ) return reject( "Outside the factory floor." );
        if ( m_initializing ) return true;
        const vecsEntity old = at( x, y );
        const Building* b = old != VECS_INVALID_ENTITY ? vecsGet<Building>( m_world, old ) : nullptr;
        if ( b && b->locked ) return reject( "The freight dock is protected. Route its requested cargo here." );
        if ( !m_rules.campaign ) return true;
        if ( m_phase == Phase::Won || m_phase == Phase::Lost ) return reject( "This dispatch is over. Replay to build again." );
        if ( tool == Tool::Erase )
        {
            if ( b && b->tool == Tool::Generator && m_stats.powerUsed > m_stats.powerLimit - 12 ) return reject( "This generator powers installed machines. Remove some machines first." );
            return true;
        }
        if ( blocked( x, y ) ) return reject( "Flooded or damaged floor. Route around it." );
        if ( !( m_rules.unlocked & toolBit( tool ) ) ) return reject( "This part unlocks in a later chapter." );
        if ( tool == Tool::Shipping ) return reject( "Use the existing freight docks." );
        if ( b && b->tool == tool ) return true;
        if ( tool == Tool::Miner && oreAt( x, y ) == 0 ) return reject( "A miner needs an unspent ore deposit." );
        const int refund = b ? refundFor( *b ) : 0;
        if ( m_stats.credits + refund < costFor( tool ) ) return reject( "Not enough credits. Ship an order or recover unused parts." );
        const int limit = m_stats.powerLimit - ( b && b->tool == Tool::Generator ? 12 : 0 ) + ( tool == Tool::Generator ? 12 : 0 );
        if ( m_stats.powerUsed - ( b ? powerFor( b->tool ) : 0 ) + powerFor( tool ) > limit ) return reject( "Not enough power. Recover a machine or build a generator (+12)." );
        return true;
    }
    bool place( int x, int y, Tool tool, int direction )
    {
        if ( !canPlace( x, y, tool ) ) return false;
        if ( tool == Tool::Erase ) return erase( x, y );
        direction = ( direction % 4 + 4 ) % 4;
        vecsEntity e = at( x, y );
        if ( e != VECS_INVALID_ENTITY && vecsGet<Building>( m_world, e )->tool == tool )
        { vecsGet<Cell>( m_world, e )->direction = direction; return true; }
        if ( e != VECS_INVALID_ENTITY && !erase( x, y ) ) return false;
        if ( m_rules.campaign && !m_initializing ) { m_stats.credits -= costFor( tool ); m_stats.spent += costFor( tool ); }
        e = vecsCreate( m_world ); m_grid[index( x, y )] = e;
        vecsSet<Cell>( m_world, e, { x, y, direction } ); vecsSet<Building>( m_world, e, { tool, false, m_rules.campaign && !m_initializing ? costFor( tool ) : 0 } );
        if ( isTransport( tool ) )
        {
            vecsSet<Conveyor>( m_world, e, { 3.0f * ( 1 + 0.25f * m_rules.beltTier ) } );
            if ( tool != Tool::Belt ) vecsSet<Router>( m_world, e, { tool == Tool::Splitter } );
        }
        else if ( tool == Tool::Miner ) vecsSet<Producer>( m_world, e, {} );
        else if ( isProcessor( tool ) ) { vecsSet<Processor>( m_world, e, { tool } ); vecsSet<Inventory>( m_world, e, {} ); }
        else if ( tool == Tool::Shipping ) vecsAddTag<Shipping>( m_world, e );
        refreshCounts(); return true;
    }
    bool erase( int x, int y )
    {
        if ( !canPlace( x, y, Tool::Erase ) ) return false;
        const vecsEntity e = at( x, y ); if ( e == VECS_INVALID_ENTITY ) return false;
        const Building building = *vecsGet<Building>( m_world, e );
        const Tool tool = building.tool;
        // A generator cannot be removed while its power is committed elsewhere.
        if ( m_rules.campaign && !m_initializing && tool == Tool::Generator && m_stats.powerUsed > m_stats.powerLimit - 12 ) return false;
        m_deaths.clear();
        parcels( [&]( vecsEntity p, Parcel& parcel, Transit& t ) { if ( t.x == x && t.y == y ) { m_deaths.push_back( p ); m_stats.scrapped += weight( parcel.kind ); } } );
        for ( vecsEntity p : m_deaths ) vecsDestroy( m_world, p ); m_deaths.clear();
        if ( const Inventory* inv = vecsGet<Inventory>( m_world, e ) ) m_stats.scrapped += inv->stored * weight( inputFor( tool ) ) + inv->plates;
        if ( const Processor* p = vecsGet<Processor>( m_world, e ) ) m_stats.scrapped += ( p->working || p->ready ) ? weight( outputFor( tool ) ) : 0;
        if ( m_rules.campaign && !m_initializing ) { const int refund = refundFor( building ); m_stats.credits += refund; m_stats.refunds += refund; }
        vecsDestroy( m_world, e ); m_grid[index( x, y )] = VECS_INVALID_ENTITY; refreshCounts(); return true;
    }
    void cycleFilter( int x, int y )
    {
        if ( const auto e = at( x, y ); e != VECS_INVALID_ENTITY ) if ( auto* r = vecsGet<Router>( m_world, e ) )
            if ( !r->splitter ) r->filter = static_cast<Item>( ( static_cast<int>( r->filter ) + 1 ) % ItemCount );
    }
    void step()
    {
        if ( m_rules.campaign && m_phase != Phase::Running ) return;
        m_stats.elapsed += FixedStep; m_stats.waiting = 0;
        std::array<bool, Width * Height> reserved{};
        parcels( [&]( vecsEntity, Parcel&, Transit& t ) { reserved[index( t.x, t.y )] = true; } );
        m_deaths.clear(); m_births.clear(); m_states.clear();
        orderedEach<Parcel, Transit>( m_parcels, [&]( vecsEntity e, Parcel& parcel, Transit& t )
        {
            const vecsEntity current = at( t.x, t.y );
            const Conveyor* belt = vecsGet<Conveyor>( m_world, current ); assert( belt );
            t.progress = std::min( 1.0f, t.progress + FixedStep * belt->speed ); if ( t.progress < 1 ) return;
            const Cell& cell = *vecsGet<Cell>( m_world, current );
            Router* router = vecsGet<Router>( m_world, current );
            const bool preferredLeft = router && ( router->splitter ? router->nextLeft : parcel.kind != router->filter );
            for ( int attempt = 0; attempt < ( router && router->splitter ? 2 : 1 ); ++attempt )
            {
                const bool left = attempt ? !preferredLeft : preferredLeft;
                const int direction = ( cell.direction + ( left ? 3 : 0 ) ) % 4;
                const int nx = t.x + DX[direction], ny = t.y + DY[direction];
                const vecsEntity target = at( nx, ny ); bool accepted = false;
                if ( target != VECS_INVALID_ENTITY )
                {
                    if ( vecsHas<Shipping>( m_world, target ) )
                    {
                        const Dock* dock = vecsGet<Dock>( m_world, target );
                        const int kind = static_cast<int>( parcel.kind );
                        if ( ( !dock || ( dock->mask & itemBit( parcel.kind ) ) ) && ( !m_rules.campaign || m_stats.delivered[kind] < m_rules.goals[kind] ) )
                        {
                            ++m_stats.delivered[kind]; m_shipments.push_back( m_stats.elapsed ); accepted = true;
                            if ( m_rules.campaign ) m_stats.credits += priceFor( parcel.kind );
                        }
                    }
                    else if ( Processor* processor = vecsGet<Processor>( m_world, target ) )
                    {
                        Inventory& inv = *vecsGet<Inventory>( m_world, target );
                        if ( parcel.kind == inputFor( processor->recipe ) && inv.stored < BufferCapacity ) { ++inv.stored; accepted = true; }
                        else if ( m_rules.campaign && processor->recipe == Tool::Assembler && parcel.kind == Item::Plate && inv.plates < BufferCapacity ) { ++inv.plates; accepted = true; }
                    }
                    else if ( vecsHas<Conveyor>( m_world, target ) && !reserved[index( nx, ny )] )
                    {
                        reserved[index( nx, ny )] = true; t.fromX = t.x; t.fromY = t.y; t.x = nx; t.y = ny; t.progress = 0;
                        if ( router && router->splitter ) router->nextLeft = !left;
                        return;
                    }
                }
                if ( accepted ) { m_deaths.push_back( e ); if ( router && router->splitter ) router->nextLeft = !left; return; }
            }
            ++m_stats.waiting;
        } );
        orderedEach<Cell, Producer>( m_producers, [&]( vecsEntity, Cell& cell, Producer& producer )
        {
            producer.cooldown = std::max( 0.0f, producer.cooldown - FixedStep );
            if ( producer.cooldown <= 0 && ( !m_rules.campaign || m_ore[index( cell.x, cell.y )] ) && emit( cell, Item::Ore, reserved ) )
            {
                producer.cooldown = 0.75f / ( 1 + 0.08f * m_rules.processingTier ); ++m_stats.produced;
                if ( m_rules.campaign ) --m_ore[index( cell.x, cell.y )];
            }
        } );
        orderedEach<Cell, Processor, Inventory>( m_processors, [&]( vecsEntity e, Cell& cell, Processor& p, Inventory& inv )
        {
            if ( p.ready && emit( cell, outputFor( p.recipe ), reserved ) ) p.ready = false;
            const uint32_t primary = m_rules.campaign && p.recipe != Tool::Smelter ? 2u : 1u;
            const uint32_t secondary = m_rules.campaign && p.recipe == Tool::Assembler ? 1u : 0u;
            if ( !p.ready && !p.working && inv.stored >= primary && inv.plates >= secondary )
            { inv.stored -= primary; inv.plates -= secondary; p.working = true; p.progress = 0; }
            if ( p.working )
            {
                p.progress += FixedStep;
                if ( p.progress >= workDuration( p.recipe ) ) { p.progress = workDuration( p.recipe ); p.working = false; p.ready = true; }
            }
            m_states.push_back( { e, p.working } );
        } );
        // Flush structural intents after all query callbacks return.
        for ( vecsEntity e : m_deaths ) vecsDestroy( m_world, e );
        for ( const Birth& b : m_births )
        {
            const vecsEntity e = vecsCreate( m_world ); vecsSet<Parcel>( m_world, e, { b.kind } );
            vecsSet<Transit>( m_world, e, { b.x, b.y, b.fromX, b.fromY, 0 } );
        }
        for ( const State& s : m_states )
        {
            if ( s.working && !vecsHas<Working>( m_world, s.entity ) ) vecsAddTag<Working>( m_world, s.entity );
            else if ( !s.working && vecsHas<Working>( m_world, s.entity ) ) vecsUnset<Working>( m_world, s.entity );
        }
        while ( !m_shipments.empty() && m_shipments.front() <= m_stats.elapsed - 60 ) m_shipments.pop_front();
        m_stats.perMinute = static_cast<float>( m_shipments.size() * 60.0 / std::min( 60.0, std::max( 1.0, m_stats.elapsed ) ) );
        refreshCounts(); evaluate();
    }
    uint32_t materialInFactory()
    {
        uint32_t count = 0;
        parcels( [&]( vecsEntity, Parcel& p, Transit& ) { count += weight( p.kind ); } );
        vecsQueryEach<Cell, Processor, Inventory>( m_world, m_processors, [&]( vecsEntity, Cell&, Processor& p, Inventory& inv )
        { count += inv.stored * weight( inputFor( p.recipe ) ) + inv.plates + ( p.working || p.ready ? weight( outputFor( p.recipe ) ) : 0 ); } );
        return count;
    }
    int stars() const { return m_phase == Phase::Won ? 1 + ( m_stats.elapsed <= m_rules.parTime ? 1 : 0 ) + ( m_stats.scrapped == 0 && m_stats.spent - m_stats.refunds <= m_rules.parCost ? 1 : 0 ) : 0; }
    FactoryState capture()
    {
        FactoryState state; state.scenario = m_rules; state.stats = m_stats; state.phase = m_phase; state.remainingOre = m_ore;
        state.shipments.assign( m_shipments.begin(), m_shipments.end() );
        buildings( [&]( vecsEntity e, Cell& cell, Building& building )
        {
            BuildingState b; b.cell = cell; b.building = building;
            if ( auto* p = vecsGet<Producer>( m_world, e ) ) b.cooldown = p->cooldown;
            if ( auto* p = vecsGet<Processor>( m_world, e ) ) { b.progress = p->progress; b.working = p->working; b.ready = p->ready; }
            if ( auto* i = vecsGet<Inventory>( m_world, e ) ) { b.stored = i->stored; b.plates = i->plates; }
            if ( auto* r = vecsGet<Router>( m_world, e ) ) { b.nextLeft = r->nextLeft; b.filter = r->filter; }
            if ( auto* d = vecsGet<Dock>( m_world, e ) ) b.dockMask = d->mask;
            state.buildings.push_back( b );
        } );
        parcels( [&]( vecsEntity, Parcel& p, Transit& t ) { state.parcels.push_back( { p.kind, t } ); } ); return state;
    }
    static bool valid( const FactoryState& state )
    {
        const auto& rules = state.scenario;
        if ( rules.credits < 0 || rules.credits > 10000000 || rules.power < 0 || rules.power > 100000 || !std::isfinite( rules.deadline ) || rules.deadline <= 0 || rules.deadline > 1e8 || !std::isfinite( rules.parTime ) || rules.parTime < 0 || rules.parTime > 1e8 || rules.parCost < 0 || rules.parCost > 10000000 ) return false;
        for ( int tier : { rules.beltTier, rules.processingTier, rules.recoveryTier } ) if ( tier < 0 || tier > 3 ) return false;
        uint64_t requested = 0; for ( uint32_t goal : rules.goals ) { if ( goal > 100000 ) return false; requested += goal; }
        if ( rules.campaign && !requested ) return false;
        if ( state.buildings.size() > Width * Height || state.parcels.size() > Width * Height || state.shipments.size() > 40000 ) return false;
        if ( !std::isfinite( state.stats.elapsed ) || state.stats.elapsed < 0 || state.stats.elapsed > 1e8 || state.stats.credits < 0 || state.stats.spent < 0 || state.stats.refunds < 0 || state.stats.refunds > state.stats.spent ) return false;
        const int phase = static_cast<int>( state.phase );
        if ( phase < 0 || phase > 4 || ( rules.campaign ? state.phase == Phase::Sandbox : state.phase != Phase::Sandbox ) ) return false;
        std::array<const BuildingState*, Width * Height> cells{}; std::array<bool, Width * Height> items{};
        uint64_t material = state.stats.scrapped, initial = 0, remaining = 0;
        const auto weight = [&]( Item i ) -> uint32_t { constexpr uint32_t w[] = { 1, 1, 2, 5 }; return rules.campaign ? w[static_cast<int>( i )] : 1; };
        for ( int i = 0; i < ItemCount; ++i ) material += static_cast<uint64_t>( state.stats.delivered[i] ) * weight( static_cast<Item>( i ) );
        int power = 0, capacity = rules.power;
        for ( const auto& b : state.buildings )
        {
            const int tool = static_cast<int>( b.building.tool ), filter = static_cast<int>( b.filter );
            if ( !inside( b.cell.x, b.cell.y ) || b.cell.direction < 0 || b.cell.direction > 3 || tool < 0 || tool >= ToolCount || b.building.tool == Tool::Erase || filter < 0 || filter >= ItemCount || b.dockMask > 15 ) return false;
            if ( b.building.paid < 0 || b.building.paid > costFor( b.building.tool ) ) return false;
            const int ix = index( b.cell.x, b.cell.y ); if ( cells[ix] || rules.blocked[ix] ) return false; cells[ix] = &b;
            if ( !std::isfinite( b.cooldown ) || b.cooldown < 0 || b.cooldown > 1 || !std::isfinite( b.progress ) || b.progress < 0 || b.progress > 10 || b.stored > BufferCapacity || b.plates > BufferCapacity || ( b.working && b.ready ) ) return false;
            if ( isProcessor( b.building.tool ) ) material += b.stored * weight( inputFor( b.building.tool ) ) + b.plates + ( b.working || b.ready ? weight( outputFor( b.building.tool ) ) : 0 );
            else if ( b.stored || b.plates || b.working || b.ready || b.progress ) return false;
            power += powerFor( b.building.tool ); if ( b.building.tool == Tool::Generator ) capacity += 12;
        }
        if ( rules.campaign && power > capacity ) return false;
        for ( const auto& dock : rules.docks )
        {
            if ( !inside( dock.x, dock.y ) ) return false;
            const auto* b = cells[index( dock.x, dock.y )];
            if ( !b || b->building.tool != Tool::Shipping || !b->building.locked || b->dockMask != dock.mask ) return false;
        }
        for ( const auto& p : state.parcels )
        {
            const auto& t = p.transit; const int kind = static_cast<int>( p.kind );
            if ( kind < 0 || kind >= ItemCount || !inside( t.x, t.y ) || !inside( t.fromX, t.fromY ) || std::abs( t.x - t.fromX ) + std::abs( t.y - t.fromY ) != 1 || !std::isfinite( t.progress ) || t.progress < 0 || t.progress > 1 ) return false;
            const int ix = index( t.x, t.y ); if ( items[ix] || !cells[ix] || !isTransport( cells[ix]->building.tool ) ) return false;
            items[ix] = true; material += weight( p.kind );
        }
        for ( int i = 0; i < Width * Height; ++i ) { if ( rules.ore[i] > 1000000 ) return false; initial += rules.ore[i]; remaining += state.remainingOre[i]; if ( state.remainingOre[i] > rules.ore[i] ) return false; }
        if ( material != state.stats.produced || ( rules.campaign && initial != remaining + state.stats.produced ) ) return false;
        if ( rules.campaign )
        {
            int64_t credits = static_cast<int64_t>( rules.credits ) - state.stats.spent + state.stats.refunds;
            bool complete = true; uint64_t needed = 0;
            for ( int i = 0; i < ItemCount; ++i )
            {
                if ( state.stats.delivered[i] > rules.goals[i] ) return false;
                credits += static_cast<int64_t>( state.stats.delivered[i] ) * priceFor( static_cast<Item>( i ) );
                complete &= state.stats.delivered[i] == rules.goals[i];
                needed += static_cast<uint64_t>( rules.goals[i] - state.stats.delivered[i] ) * weight( static_cast<Item>( i ) );
            }
            if ( credits != state.stats.credits ) return false;
            if ( state.phase == Phase::Won && ( !complete || state.stats.elapsed <= 0 ) ) return false;
            if ( state.phase == Phase::Running && ( complete || state.stats.elapsed >= rules.deadline ) ) return false;
            if ( state.phase == Phase::Planning && ( state.stats.elapsed != 0 || state.stats.produced != 0 ) ) return false;
            const uint64_t inFactory = material - state.stats.scrapped - [&]() { uint64_t shipped = 0; for ( int i = 0; i < ItemCount; ++i ) shipped += static_cast<uint64_t>( state.stats.delivered[i] ) * weight( static_cast<Item>( i ) ); return shipped; }();
            if ( state.phase == Phase::Lost && state.stats.elapsed < rules.deadline && needed <= remaining + inFactory ) return false;
        }
        double previous = -1;
        for ( double shipment : state.shipments ) { if ( !std::isfinite( shipment ) || shipment < 0 || shipment > state.stats.elapsed || shipment < previous ) return false; previous = shipment; }
        return true;
    }
    bool restore( const FactoryState& state )
    {
        if ( !valid( state ) ) return false;
        reset( false ); m_rules = state.scenario; m_initializing = true;
        for ( const auto& b : state.buildings )
        {
            place( b.cell.x, b.cell.y, b.building.tool, b.cell.direction ); const auto e = at( b.cell.x, b.cell.y );
            *vecsGet<Building>( m_world, e ) = b.building;
            if ( auto* p = vecsGet<Producer>( m_world, e ) ) p->cooldown = b.cooldown;
            if ( auto* p = vecsGet<Processor>( m_world, e ) ) { p->progress = b.progress; p->working = b.working; p->ready = b.ready; if ( p->working ) vecsAddTag<Working>( m_world, e ); }
            if ( auto* i = vecsGet<Inventory>( m_world, e ) ) { i->stored = b.stored; i->plates = b.plates; }
            if ( auto* r = vecsGet<Router>( m_world, e ) ) { r->nextLeft = b.nextLeft; r->filter = b.filter; }
            if ( b.building.tool == Tool::Shipping ) vecsSet<Dock>( m_world, e, { b.dockMask } );
        }
        for ( const auto& p : state.parcels ) { const auto e = vecsCreate( m_world ); vecsSet<Parcel>( m_world, e, { p.kind } ); vecsSet<Transit>( m_world, e, p.transit ); }
        m_initializing = false; m_stats = state.stats; m_ore = state.remainingOre; m_phase = state.phase;
        m_shipments.assign( state.shipments.begin(), state.shipments.end() ); refreshCounts();
        if ( m_phase == Phase::Lost ) m_failure = m_stats.elapsed >= m_rules.deadline ? "The convoy departed before the order was ready." : "There is not enough ore left to complete the order.";
        return true;
    }
private:
    struct Birth { Item kind; int x, y, fromX, fromY; };
    struct State { vecsEntity entity; bool working; };
    vecsWorld* m_world = nullptr;
    vecsQuery *m_buildings = nullptr, *m_parcels = nullptr, *m_producers = nullptr, *m_processors = nullptr, *m_working = nullptr;
    std::array<vecsEntity, Width * Height> m_grid{};
    std::array<uint32_t, Width * Height> m_ore{};
    Scenario m_rules; Stats m_stats; Phase m_phase = Phase::Sandbox; std::string m_failure;
    bool m_initializing = false;
    std::deque<double> m_shipments; std::vector<Birth> m_births; std::vector<vecsEntity> m_deaths; std::vector<State> m_states;
    template<typename... Components, typename Fn>
    void orderedEach( vecsQuery* query, Fn&& callback )
    {
        // Game priority is spatial, not an implementation detail of entity IDs.
        // Recycled handles after save/load must not change merge winners.
        struct Hit { vecsEntity entity = 0; std::tuple<Components*...> parts{}; bool live = false; };
        std::array<Hit, Width * Height> ordered{};
        vecsQueryEach<Components...>( m_world, query, [&]( vecsEntity e, Components&... components )
        {
            const auto parts = std::tuple<Components*...>{ &components... };
            int ix = 0;
            if constexpr ( ( std::is_same_v<Components, Transit> || ... ) )
            { const auto* t = std::get<Transit*>( parts ); ix = index( t->x, t->y ); }
            else { const auto* c = std::get<Cell*>( parts ); ix = index( c->x, c->y ); }
            ordered[ix] = { e, parts, true };
        } );
        for ( const auto& hit : ordered ) if ( hit.live ) std::apply( [&]( auto*... components ) { callback( hit.entity, *components... ); }, hit.parts );
    }
    bool emit( const Cell& cell, Item kind, std::array<bool, Width * Height>& reserved )
    {
        const int x = cell.x + DX[cell.direction], y = cell.y + DY[cell.direction]; const auto target = at( x, y );
        if ( target == VECS_INVALID_ENTITY || !vecsHas<Conveyor>( m_world, target ) || reserved[index( x, y )] ) return false;
        reserved[index( x, y )] = true; m_births.push_back( { kind, x, y, cell.x, cell.y } ); return true;
    }
    void refreshCounts()
    {
        m_stats.items = vecsQueryCount( m_world, m_parcels ); m_stats.machinesWorking = vecsQueryCount( m_world, m_working );
        m_stats.waiting = std::min( m_stats.waiting, m_stats.items ); m_stats.powerUsed = 0; m_stats.powerLimit = m_rules.power;
        buildings( [&]( vecsEntity, Cell&, Building& b ) { m_stats.powerUsed += powerFor( b.tool ); if ( b.tool == Tool::Generator ) m_stats.powerLimit += 12; } );
    }
    void evaluate()
    {
        if ( !m_rules.campaign ) return;
        bool complete = true; uint32_t needed = 0;
        for ( int i = 0; i < ItemCount; ++i ) { complete &= m_stats.delivered[i] >= m_rules.goals[i]; needed += ( m_rules.goals[i] - std::min( m_rules.goals[i], m_stats.delivered[i] ) ) * weight( static_cast<Item>( i ) ); }
        if ( complete ) m_phase = Phase::Won;
        else if ( m_stats.elapsed >= m_rules.deadline ) { m_phase = Phase::Lost; m_failure = "The convoy departed before the order was ready. Shorten routes or add parallel machines."; }
        else if ( needed > oreLeft() + materialInFactory() ) { m_phase = Phase::Lost; m_failure = "There is not enough ore left to finish the order. Retry and protect material in buffers and conveyors."; }
    }
    void release()
    {
        if ( !m_world ) return;
        vecsDestroyQuery( m_buildings ); vecsDestroyQuery( m_parcels ); vecsDestroyQuery( m_producers ); vecsDestroyQuery( m_processors ); vecsDestroyQuery( m_working ); vecsDestroyWorld( m_world ); m_world = nullptr;
    }
};
}
