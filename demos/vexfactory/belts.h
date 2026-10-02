#pragma once
#include "simulation.h"

namespace vexfactory
{
inline bool placePart( Simulation& sim, int x, int y, Tool tool, int direction, bool replace, std::string* reason = nullptr )
{
    const auto existing = sim.at( x, y );
    if ( existing != VECS_INVALID_ENTITY && tool != Tool::Erase && vecsGet<Building>( sim.world(), existing )->tool != tool && !replace )
    { if ( reason ) *reason = "Occupied. Shift-click replaces a part; right-click demolishes it."; return false; }
    return sim.canPlace( x, y, tool, reason ) && sim.place( x, y, tool, direction );
}

// Only the current stroke is rewired. Nearby production lines keep their flow.
// Every step is cardinal, even when the pointer skips cells between frames.
class BeltStroke
{
    int m_x = -1, m_y = -1;
public:
    void reset() { m_x = m_y = -1; }
    bool active() const { return Simulation::inside( m_x, m_y ); }
    bool paint( Simulation& sim, int x, int y, int& direction, bool replace = false, std::string* reason = nullptr )
    {
        if ( !Simulation::inside( x, y ) ) { reset(); return false; }
        if ( !active() )
        {
            if ( !placePart( sim, x, y, Tool::Belt, direction, replace, reason ) ) return false;
            m_x = x; m_y = y; return true;
        }
        bool changed = false;
        while ( m_x != x || m_y != y )
        {
            const int step = m_x != x ? ( x > m_x ? 0 : 2 ) : ( y > m_y ? 1 : 3 );
            const int nx = m_x + DX[step], ny = m_y + DY[step];
            const auto target = sim.at( nx, ny );
            const Building* building = target != VECS_INVALID_ENTITY ? vecsGet<Building>( sim.world(), target ) : nullptr;
            // Finish at a machine input or protected dock, without replacing it.
            const bool sink = building && !replace && ( isProcessor( building->tool ) || building->tool == Tool::Shipping );
            if ( !sink && !placePart( sim, nx, ny, Tool::Belt, step, replace, reason ) ) { reset(); break; }
            const auto previous = sim.at( m_x, m_y );
            if ( previous != VECS_INVALID_ENTITY && vecsGet<Building>( sim.world(), previous )->tool == Tool::Belt )
                changed |= sim.place( m_x, m_y, Tool::Belt, step );
            direction = step;
            if ( sink ) { reset(); break; }
            changed = true; m_x = nx; m_y = ny;
        }
        return changed;
    }
};

// Entry is a side of the tile (E/S/W/N), not a flow direction. Ambiguous
// merges stay straight; never infer or change a belt's output from adjacency.
inline int beltEntry( const Simulation& sim, int x, int y )
{
    int entry = -1;
    for ( int side = 0; side < 4; ++side )
    {
        const auto e = sim.at( x + DX[side], y + DY[side] ); if ( e == VECS_INVALID_ENTITY ) continue;
        const auto& b = *vecsGet<Building>( sim.world(), e ); const auto& c = *vecsGet<Cell>( sim.world(), e );
        const bool source = isTransport( b.tool ) || isProcessor( b.tool ) || b.tool == Tool::Miner;
        const int toward = ( side + 2 ) % 4;
        if ( !source || ( c.direction != toward && !( vecsHas<Router>( sim.world(), e ) && ( c.direction + 3 ) % 4 == toward ) ) ) continue;
        if ( entry != -1 ) return -1;
        entry = side;
    }
    return entry;
}
inline bool beltCorner( int entry, int output ) { return entry >= 0 && entry != output && entry != ( output + 2 ) % 4; }
struct BeltPoint { float x, y; };
inline BeltPoint cornerPoint( int entry, int output, float progress )
{
    const float angle = std::clamp( progress, 0.0f, 1.0f ) * 1.57079632679f;
    return { .5f + .5f * ( DX[entry] + DX[output] - DX[output] * std::cos( angle ) - DX[entry] * std::sin( angle ) ),
             .5f + .5f * ( DY[entry] + DY[output] - DY[output] * std::cos( angle ) - DY[entry] * std::sin( angle ) ) };
}
inline BeltPoint parcelPoint( const Simulation& sim, const Transit& t )
{
    BeltPoint point{ t.fromX + ( t.x - t.fromX ) * t.progress + .5f, t.fromY + ( t.y - t.fromY ) * t.progress + .5f };
    const bool leaving = t.progress < .5f;
    const int x = leaving ? t.fromX : t.x, y = leaving ? t.fromY : t.y;
    const auto e = sim.at( x, y ); if ( e == VECS_INVALID_ENTITY || vecsGet<Building>( sim.world(), e )->tool != Tool::Belt ) return point;
    const int output = vecsGet<Cell>( sim.world(), e )->direction, entry = beltEntry( sim, x, y );
    if ( !beltCorner( entry, output ) ) return point;
    if ( leaving ? x + DX[output] != t.x || y + DY[output] != t.y : x + DX[entry] != t.fromX || y + DY[entry] != t.fromY ) return point;
    const auto curve = cornerPoint( entry, output, leaving ? .5f + t.progress : t.progress - .5f );
    return { x + curve.x, y + curve.y };
}
}
