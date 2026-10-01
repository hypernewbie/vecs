#pragma once

#include "vecs.h"
#include <algorithm>
#include <array>
#include <deque>
#include <utility>
#include <vector>

namespace vexfactory
{
constexpr int Width = 24;
constexpr int Height = 16;
constexpr float FixedStep = 1.0f / 60.0f;
constexpr uint32_t BufferCapacity = 4;
constexpr int DX[] = { 1, 0, -1, 0 };
constexpr int DY[] = { 0, 1, 0, -1 };

enum class Tool { Belt, Miner, Smelter, Press, Assembler, Shipping, Erase };
enum class Item { Ore, Plate, Gear, Engine, Count };
constexpr int ItemCount = static_cast<int>( Item::Count );

inline const char* toolName( Tool tool )
{
    constexpr const char* names[] = { "Conveyor", "Ore miner", "Smelter", "Gear press", "Assembler", "Shipping", "Erase" };
    return names[static_cast<int>( tool )];
}
inline const char* itemName( Item item )
{
    constexpr const char* names[] = { "Ore", "Plates", "Gears", "Engines" };
    return names[static_cast<int>( item )];
}
inline Item inputFor( Tool tool ) { return tool == Tool::Smelter ? Item::Ore : tool == Tool::Press ? Item::Plate : Item::Gear; }
inline Item outputFor( Tool tool ) { return static_cast<Item>( static_cast<int>( inputFor( tool ) ) + 1 ); }
inline float durationFor( Tool tool ) { return tool == Tool::Smelter ? 1.1f : tool == Tool::Press ? 1.5f : 2.0f; }

// All simulation state lives in Vecs components. The grid stores handles only.
struct Cell { int x, y, direction; };
struct Building { Tool tool; };
struct Conveyor { float speed = 3.0f; };
struct Producer { float cooldown = 0.0f; };
struct Inventory { uint32_t stored = 0; };
struct Processor { Tool recipe; float progress = 0.0f; bool working = false; bool ready = false; };
struct Shipping {};
struct Working {};
struct Parcel { Item kind; };
struct Transit { int x, y, fromX, fromY; float progress; };

struct Stats
{
    uint32_t produced = 0;
    uint32_t scrapped = 0;
    std::array<uint32_t, ItemCount> delivered{};
    uint32_t items = 0;
    uint32_t waiting = 0;
    uint32_t machinesWorking = 0;
    double elapsed = 0.0;
    float perMinute = 0.0f;
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
    vecsEntity at( int x, int y ) const { return inside( x, y ) ? m_grid[index( x, y )] : VECS_INVALID_ENTITY; }
    uint32_t entities() const { return vecsCount( m_world ); }

    template<typename Fn> void buildings( Fn&& fn )
    {
        vecsQueryEach<Cell, Building>( m_world, m_buildings, std::forward<Fn>( fn ) );
    }
    template<typename Fn> void parcels( Fn&& fn )
    {
        vecsQueryEach<Parcel, Transit>( m_world, m_parcels, std::forward<Fn>( fn ) );
    }

    void reset( bool starter )
    {
        release();
        m_world = vecsCreateWorld( 4096 );
        m_buildings = vecsBuildQuery<Cell, Building>( m_world );
        m_parcels = vecsBuildQuery<Parcel, Transit>( m_world );
        m_producers = vecsBuildQuery<Cell, Producer>( m_world );
        m_processors = vecsBuildQuery<Cell, Processor, Inventory>( m_world );
        m_working = vecsBuildQuery<Working>( m_world );
        m_grid.fill( VECS_INVALID_ENTITY );
        m_stats = {};
        m_shipments.clear();
        m_births.clear();
        m_deaths.clear();
        m_states.clear();
        if ( starter )
        {
            // Three complete production lines. The remaining floor is a sandbox.
            for ( int lane = 0; lane < 3; ++lane )
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
    }

    bool place( int x, int y, Tool tool, int direction )
    {
        if ( !inside( x, y ) ) return false;
        if ( tool == Tool::Erase ) return erase( x, y );
        direction = ( direction % 4 + 4 ) % 4;
        vecsEntity e = at( x, y );
        if ( e != VECS_INVALID_ENTITY && vecsGet<Building>( m_world, e )->tool == tool )
        {
            // Rotating an existing machine preserves its work and inventory.
            vecsGet<Cell>( m_world, e )->direction = direction;
            return true;
        }
        erase( x, y );
        e = vecsCreate( m_world );
        m_grid[index( x, y )] = e;
        vecsSet<Cell>( m_world, e, { x, y, direction } );
        vecsSet<Building>( m_world, e, { tool } );
        switch ( tool )
        {
            case Tool::Belt: vecsSet<Conveyor>( m_world, e, {} ); break;
            case Tool::Miner: vecsSet<Producer>( m_world, e, {} ); break;
            case Tool::Smelter:
            case Tool::Press:
            case Tool::Assembler:
                vecsSet<Processor>( m_world, e, { tool } );
                vecsSet<Inventory>( m_world, e, {} );
                break;
            case Tool::Shipping: vecsAddTag<Shipping>( m_world, e ); break;
            case Tool::Erase: break;
        }
        refreshCounts();
        return true;
    }

    bool erase( int x, int y )
    {
        if ( !inside( x, y ) ) return false;
        const vecsEntity e = at( x, y );
        if ( e == VECS_INVALID_ENTITY ) return false;
        m_deaths.clear();
        parcels( [&]( vecsEntity p, Parcel&, Transit& t )
        {
            if ( t.x == x && t.y == y ) m_deaths.push_back( p );
        } );
        for ( vecsEntity p : m_deaths ) { vecsDestroy( m_world, p ); ++m_stats.scrapped; }
        m_deaths.clear();
        if ( const Inventory* inv = vecsGet<Inventory>( m_world, e ) ) m_stats.scrapped += inv->stored;
        if ( const Processor* proc = vecsGet<Processor>( m_world, e ) )
            m_stats.scrapped += ( proc->working || proc->ready ) ? 1u : 0u;
        vecsDestroy( m_world, e );
        m_grid[index( x, y )] = VECS_INVALID_ENTITY;
        refreshCounts();
        return true;
    }

    // Call only at fixed 60 Hz. Rendering and editing run outside this step.
    void step()
    {
        m_stats.elapsed += FixedStep;
        m_stats.waiting = 0;
        std::array<bool, Width * Height> reserved{};
        parcels( [&]( vecsEntity, Parcel&, Transit& t ) { reserved[index( t.x, t.y )] = true; } );
        m_deaths.clear();
        m_births.clear();
        m_states.clear();

        parcels( [&]( vecsEntity e, Parcel& parcel, Transit& t )
        {
            const Conveyor* belt = vecsGet<Conveyor>( m_world, at( t.x, t.y ) );
            assert( belt );
            t.progress = std::min( 1.0f, t.progress + FixedStep * belt->speed );
            if ( t.progress < 1.0f ) return;
            const Cell& cell = *vecsGet<Cell>( m_world, at( t.x, t.y ) );
            const int nx = t.x + DX[cell.direction], ny = t.y + DY[cell.direction];
            const vecsEntity target = at( nx, ny );
            bool accepted = false;
            if ( target != VECS_INVALID_ENTITY )
            {
                if ( vecsHas<Shipping>( m_world, target ) )
                {
                    ++m_stats.delivered[static_cast<int>( parcel.kind )];
                    m_shipments.push_back( m_stats.elapsed );
                    accepted = true;
                }
                else if ( Processor* processor = vecsGet<Processor>( m_world, target ) )
                {
                    Inventory& inv = *vecsGet<Inventory>( m_world, target );
                    if ( parcel.kind == inputFor( processor->recipe ) && inv.stored < BufferCapacity )
                    {
                        ++inv.stored;
                        accepted = true;
                    }
                }
                else if ( vecsHas<Conveyor>( m_world, target ) && !reserved[index( nx, ny )] )
                {
                    // Snapshot occupancy stays reserved for the whole step. This
                    // prevents swaps, collisions, and iteration-order chain jumps.
                    reserved[index( nx, ny )] = true;
                    t.fromX = t.x; t.fromY = t.y;
                    t.x = nx; t.y = ny; t.progress = 0.0f;
                    return;
                }
            }
            if ( accepted ) m_deaths.push_back( e );
            else ++m_stats.waiting;
        } );

        vecsQueryEach<Cell, Producer>( m_world, m_producers, [&]( vecsEntity, Cell& cell, Producer& producer )
        {
            producer.cooldown = std::max( 0.0f, producer.cooldown - FixedStep );
            if ( producer.cooldown <= 0.0f && emit( cell, Item::Ore, reserved ) )
            {
                producer.cooldown = 0.75f;
                ++m_stats.produced;
            }
        } );
        vecsQueryEach<Cell, Processor, Inventory>( m_world, m_processors,
            [&]( vecsEntity e, Cell& cell, Processor& processor, Inventory& inv )
        {
            if ( processor.ready && emit( cell, outputFor( processor.recipe ), reserved ) ) processor.ready = false;
            if ( !processor.ready && !processor.working && inv.stored > 0 )
            {
                --inv.stored;
                processor.working = true;
                processor.progress = 0.0f;
            }
            if ( processor.working )
            {
                processor.progress += FixedStep;
                if ( processor.progress >= durationFor( processor.recipe ) )
                {
                    processor.progress = durationFor( processor.recipe );
                    processor.working = false;
                    processor.ready = true;
                }
            }
            m_states.push_back( { e, processor.working } );
        } );

        // Vecs currently has no command-buffer API. Flush demo-owned intents only
        // after query callbacks return, so pool growth cannot invalidate references.
        for ( vecsEntity e : m_deaths ) vecsDestroy( m_world, e );
        for ( const Birth& birth : m_births )
        {
            const vecsEntity e = vecsCreate( m_world );
            vecsSet<Parcel>( m_world, e, { birth.kind } );
            vecsSet<Transit>( m_world, e, { birth.x, birth.y, birth.fromX, birth.fromY, 0.0f } );
        }
        for ( const State& state : m_states )
        {
            if ( state.working && !vecsHas<Working>( m_world, state.entity ) ) vecsAddTag<Working>( m_world, state.entity );
            else if ( !state.working && vecsHas<Working>( m_world, state.entity ) ) vecsUnset<Working>( m_world, state.entity );
        }
        while ( !m_shipments.empty() && m_shipments.front() <= m_stats.elapsed - 60.0 ) m_shipments.pop_front();
        m_stats.perMinute = static_cast<float>( m_shipments.size() * 60.0 / std::min( 60.0, std::max( 1.0, m_stats.elapsed ) ) );
        refreshCounts();
    }

    // One unit stays one unit through each recipe, including buffered/in-flight work.
    // This supports conservation checks without relying on component pool internals.
    uint32_t materialInFactory()
    {
        uint32_t count = vecsQueryCount( m_world, m_parcels );
        vecsQueryEach<Cell, Processor, Inventory>( m_world, m_processors,
            [&]( vecsEntity, Cell&, Processor& p, Inventory& inv ) { count += inv.stored + ( p.working || p.ready ? 1u : 0u ); } );
        return count;
    }

private:
    struct Birth { Item kind; int x, y, fromX, fromY; };
    struct State { vecsEntity entity; bool working; };
    vecsWorld* m_world = nullptr;
    vecsQuery *m_buildings = nullptr, *m_parcels = nullptr, *m_producers = nullptr, *m_processors = nullptr, *m_working = nullptr;
    std::array<vecsEntity, Width * Height> m_grid{};
    Stats m_stats{};
    std::deque<double> m_shipments;
    std::vector<Birth> m_births;
    std::vector<vecsEntity> m_deaths;
    std::vector<State> m_states;

    bool emit( const Cell& cell, Item kind, std::array<bool, Width * Height>& reserved )
    {
        const int x = cell.x + DX[cell.direction], y = cell.y + DY[cell.direction];
        const vecsEntity target = at( x, y );
        if ( target == VECS_INVALID_ENTITY || !vecsHas<Conveyor>( m_world, target ) || reserved[index( x, y )] ) return false;
        reserved[index( x, y )] = true;
        m_births.push_back( { kind, x, y, cell.x, cell.y } );
        return true;
    }
    void refreshCounts()
    {
        m_stats.items = vecsQueryCount( m_world, m_parcels );
        m_stats.machinesWorking = vecsQueryCount( m_world, m_working );
    }
    void release()
    {
        if ( !m_world ) return;
        vecsDestroyQuery( m_buildings ); vecsDestroyQuery( m_parcels ); vecsDestroyQuery( m_producers );
        vecsDestroyQuery( m_processors ); vecsDestroyQuery( m_working );
        vecsDestroyWorld( m_world );
        m_world = nullptr;
    }
};
}
