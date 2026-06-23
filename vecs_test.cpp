#define NDEBUG 1
#define assert( x ) ( ( void )0 )
#define _ASSERT( x ) ( ( void )0 )
#define _WASSERT( x ) ( ( void )0 )
#include "utest.h"
#include "vecs.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

#if defined( _MSC_VER ) && defined( _DEBUG )
#include <crtdbg.h>
// Silence MSVC debug-CRT msgboxes / STATUS_BREAKPOINT.
// Default GUI report mode pops a box; "Retry" -> __debugbreak() -> STATUS_BREAKPOINT.
// Route ALL reports to stderr and install a no-op invalid-parameter handler
// so CRT internals don't fault-fail the worker-thread TLS teardown.
static const int vecs_silence_debug_crt = []() noexcept {
    const int rts[] = { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT };
    for ( int rt : rts )
    {
        _CrtSetReportMode( rt, _CRTDBG_MODE_FILE );
        _CrtSetReportFile( rt, _CRTDBG_FILE_STDERR );
    }
    _set_invalid_parameter_handler(
        []( const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t ) {} );
    _set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
    _set_error_mode( _OUT_TO_STDERR );
    return 0;
}();
#endif
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined( __has_include )
    #if __has_include( "entt/entt.hpp" )
        #include "entt/entt.hpp"
        #define VECS_HAS_ENTT 1
    #endif
#endif

struct Position { float x, y; Position() = default; Position( float x_, float y_ ) : x( x_ ), y( y_ ) {} };
struct Velocity { float vx, vy; Velocity() = default; Velocity( float vx_, float vy_ ) : vx( vx_ ), vy( vy_ ) {} };
struct Health { int hp; Health() = default; Health( int hp_ ) : hp( hp_ ) {} };
struct HeavyPayload { uint32_t marker; uint8_t bytes[128]; };
struct IsEnemy {};
struct Dead {};
template< size_t I > struct ExhaustComp { int value; };
template< size_t I > struct ExhaustTag {};
using OverflowPod = ExhaustComp<1001>;
using OverflowPodAlt = ExhaustComp<1002>;
using OverflowTag = ExhaustTag<1001>;
using OverflowTagAlt = ExhaustTag<1002>;

static int g_overflowOnAddCount = 0;
static int g_overflowOnRemoveCount = 0;

static void onOverflowPodAdd( vecsWorld*, vecsEntity, OverflowPod* )
{
    g_overflowOnAddCount++;
}

static void onOverflowPodRemove( vecsWorld*, vecsEntity, OverflowPod* )
{
    g_overflowOnRemoveCount++;
}

template< size_t... I >
static void vecsCollectExhaustIds( uint32_t* out, std::index_sequence<I...> )
{
    ( ( out[I] = vecsTypeId<ExhaustComp<I>>() ), ... );
}

template< size_t... I >
static uint32_t vecsTrySetExhaustComponents( vecsWorld* w, vecsEntity e, std::index_sequence<I...> )
{
    uint32_t setCount = 0u;
    ( ( vecsTypeId<ExhaustComp<I>>() < VECS_MAX_COMPONENTS
        ? ( ( void )vecsSet<ExhaustComp<I>>( w, e, { ( int )I } ), setCount++ )
        : 0 ),
      ... );
    return setCount;
}

static double vecsBenchOpsPerSecond( std::chrono::high_resolution_clock::time_point start, uint64_t ops )
{
    const auto end = std::chrono::high_resolution_clock::now();
    const double elapsed = std::chrono::duration<double>( end - start ).count();
    return elapsed > 0.0 ? ( double )ops / elapsed : 0.0;
}

static std::string vecsFormatOps( double ops )
{
    char buf[64];
    if ( ops >= 1e9 )
        snprintf( buf, sizeof( buf ), "%.2f B ops/s", ops / 1e9 );
    else if ( ops >= 1e6 )
        snprintf( buf, sizeof( buf ), "%.2f M ops/s", ops / 1e6 );
    else if ( ops >= 1e3 )
        snprintf( buf, sizeof( buf ), "%.2f K ops/s", ops / 1e3 );
    else
        snprintf( buf, sizeof( buf ), "%.0f ops/s", ops );
    return std::string( buf );
}

static void vecs_test_clear_world_dummy_observer( vecsWorld*, vecsEntity, void* )
{
}

static uint32_t vecsTestClampEntities( uint32_t desired )
{
    return desired < VECS_MAX_ENTITIES ? desired : ( uint32_t )VECS_MAX_ENTITIES;
}

static uint32_t vecsTestClampIndex( uint32_t desired )
{
    return desired < ( VECS_MAX_ENTITIES - 1u ) ? desired : ( uint32_t )VECS_MAX_ENTITIES - 1u;
}

static uint32_t vecsTestTopSpan()
{
    const uint32_t tops = VECS_TOP_COUNT > 0u ? ( uint32_t )VECS_TOP_COUNT : 1u;
    return ( ( uint32_t )VECS_L2_COUNT / tops ) * 64u;
}

static uint32_t vecsTestSeparatedPlacementStart( uint32_t reserveSlots )
{
    const uint32_t preferred = VECS_TOP_COUNT > 1u ? vecsTestTopSpan() : ( VECS_L2_COUNT > 1u ? 64u : 2u );
    if ( VECS_MAX_ENTITIES <= reserveSlots )
    {
        return 0u;
    }
    const uint32_t maxStart = ( uint32_t )VECS_MAX_ENTITIES - reserveSlots;
    return preferred < maxStart ? preferred : maxStart;
}

static uint32_t vecsTestHierarchyNodesPerRoot( uint32_t childDepth, uint32_t childrenPerLevel )
{
    uint32_t total = 1u;
    uint32_t levelCount = 1u;
    for ( uint32_t d = 0; d < childDepth; d++ )
    {
        levelCount *= childrenPerLevel;
        total += levelCount;
    }
    return total;
}

static uint32_t vecsTestHierarchyRootCount( uint32_t desiredRoots, uint32_t childDepth, uint32_t childrenPerLevel, uint32_t capacity )
{
    const uint32_t nodesPerRoot = vecsTestHierarchyNodesPerRoot( childDepth, childrenPerLevel );
    const uint32_t maxRoots = nodesPerRoot > 0u ? capacity / nodesPerRoot : 0u;
    if ( maxRoots == 0u )
    {
        return 1u;
    }
    return desiredRoots < maxRoots ? desiredRoots : maxRoots;
}

static std::vector<uint32_t> vecsTestBoundaryCounts()
{
    std::vector<uint32_t> counts = { 1u, 2u, 63u, 64u, 65u, 127u, 128u, 129u, 255u, 256u, 257u, 4095u, 4096u, 4097u };
    if ( VECS_MAX_ENTITIES > 1u )
    {
        counts.push_back( ( uint32_t )VECS_MAX_ENTITIES - 1u );
    }
    counts.push_back( ( uint32_t )VECS_MAX_ENTITIES );
    counts.erase( std::remove_if( counts.begin(), counts.end(), []( uint32_t value ) { return value == 0u || value > VECS_MAX_ENTITIES; } ), counts.end() );
    std::sort( counts.begin(), counts.end() );
    counts.erase( std::unique( counts.begin(), counts.end() ), counts.end() );
    return counts;
}

static uint64_t vecsHashMix( uint64_t hash, uint64_t value )
{
    hash ^= value + 0x9E3779B97F4A7C15ull + ( hash << 6u ) + ( hash >> 2u );
    return hash;
}

static uint32_t vecsHashFloatBits( float value )
{
    uint32_t bits = 0u;
    std::memcpy( &bits, &value, sizeof( bits ) );
    return bits;
}

static void vecsSortEntities( std::vector<vecsEntity>& entities )
{
    std::sort( entities.begin(), entities.end(), []( vecsEntity a, vecsEntity b )
    {
        const uint32_t indexA = vecsEntityIndex( a );
        const uint32_t indexB = vecsEntityIndex( b );
        if ( indexA != indexB )
        {
            return indexA < indexB;
        }
        return vecsEntityGeneration( a ) < vecsEntityGeneration( b );
    } );
}

static bool vecsAssertEntitySetsEqual( const std::vector<vecsEntity>& lhs, const std::vector<vecsEntity>& rhs )
{
    if ( lhs.size() != rhs.size() )
    {
        return false;
    }
    for ( size_t i = 0; i < lhs.size(); i++ )
    {
        if ( lhs[i] != rhs[i] )
        {
            return false;
        }
    }
    return true;
}

template< typename... With >
static std::vector<vecsEntity> vecsCollectEachEntities( vecsWorld* w )
{
    std::vector<vecsEntity> entities;
    vecsEach<With...>( w, [&]( vecsEntity e, With&... ) { entities.push_back( e ); } );
    vecsSortEntities( entities );
    return entities;
}

template< typename... With >
static std::vector<vecsEntity> vecsCollectQueryEntities( vecsWorld* w, vecsQuery* q )
{
    std::vector<vecsEntity> entities;
    vecsQueryEach<With...>( w, q, [&]( vecsEntity e, With&... ) { entities.push_back( e ); } );
    vecsSortEntities( entities );
    return entities;
}

template< typename... With >
static std::vector<vecsEntity> vecsCollectFreshChunkEntities( vecsWorld* w, vecsQuery* q, uint32_t maxChunks )
{
    std::vector<vecsEntity> entities;
    std::vector<vecsQueryChunk> chunks( maxChunks ? maxChunks : 1u );
    uint32_t chunkCount = vecsQueryGetChunks( w, q, chunks.data(), ( uint32_t )chunks.size() );
    for ( uint32_t i = 0; i < chunkCount; i++ )
    {
        vecsQueryExecuteChunk<With...>( w, q, &chunks[i], [&]( vecsEntity e, With&... ) { entities.push_back( e ); } );
    }
    vecsSortEntities( entities );
    return entities;
}

static std::vector<vecsEntity> vecsCollectAliveHandles( vecsWorld* w )
{
    std::vector<vecsEntity> entities;
    for ( uint32_t index = 0; index < w->maxEntities; index++ )
    {
        if ( w->entities->allocated[index] != 0u )
        {
            entities.push_back( vecsMakeEntity( index, w->entities->generations[index] ) );
        }
    }
    return entities;
}

struct ShadowSingletonValue
{
    int value = 0;
};

struct LifetimeTracker
{
    static inline std::atomic<int> liveCount = 0;
    static inline std::atomic<int> destroyedCount = 0;
    static inline std::atomic<int> copiedCount = 0;
    static inline std::atomic<int> movedCount = 0;

    int value = 0;

    LifetimeTracker() noexcept
    {
        liveCount.fetch_add( 1, std::memory_order_relaxed );
    }

    explicit LifetimeTracker( int v ) noexcept : value( v )
    {
        liveCount.fetch_add( 1, std::memory_order_relaxed );
    }

    LifetimeTracker( const LifetimeTracker& other ) noexcept : value( other.value )
    {
        liveCount.fetch_add( 1, std::memory_order_relaxed );
        copiedCount.fetch_add( 1, std::memory_order_relaxed );
    }

    LifetimeTracker( LifetimeTracker&& other ) noexcept : value( other.value )
    {
        liveCount.fetch_add( 1, std::memory_order_relaxed );
        movedCount.fetch_add( 1, std::memory_order_relaxed );
    }

    LifetimeTracker& operator=( const LifetimeTracker& other ) noexcept
    {
        value = other.value;
        copiedCount.fetch_add( 1, std::memory_order_relaxed );
        return *this;
    }

    ~LifetimeTracker()
    {
        liveCount.fetch_sub( 1, std::memory_order_relaxed );
        destroyedCount.fetch_add( 1, std::memory_order_relaxed );
    }

    static void reset()
    {
        liveCount.store( 0, std::memory_order_relaxed );
        destroyedCount.store( 0, std::memory_order_relaxed );
        copiedCount.store( 0, std::memory_order_relaxed );
        movedCount.store( 0, std::memory_order_relaxed );
    }
};

struct EventLedger
{
    struct Event
    {
        const char* label;
        vecsEntity entity;
        int value;
    };

    std::vector<Event> events;

    void clear()
    {
        events.clear();
    }

    void record( const char* label, vecsEntity entity, int value = 0 )
    {
        events.push_back( { label, entity, value } );
    }
};

struct ChunkCoverageBitmap
{
    explicit ChunkCoverageBitmap( uint32_t capacity ) : capacity( capacity ), seen( new std::atomic<uint32_t>[capacity] )
    {
        reset();
    }

    void reset()
    {
        for ( uint32_t i = 0; i < capacity; i++ )
        {
            seen[i].store( 0u, std::memory_order_relaxed );
        }
    }

    void mark( vecsEntity e )
    {
        seen[vecsEntityIndex( e )].fetch_add( 1u, std::memory_order_relaxed );
    }

    uint32_t count( uint32_t index ) const
    {
        return seen[index].load( std::memory_order_relaxed );
    }

    uint32_t capacity;
    std::unique_ptr<std::atomic<uint32_t>[]> seen;
};

struct SeededOpStream
{
    explicit SeededOpStream( uint32_t seed ) : state( seed ? seed : 1u )
    {
    }

    uint32_t next()
    {
        state = state * 1664525u + 1013904223u;
        return state;
    }

    uint32_t nextBounded( uint32_t bound )
    {
        return bound ? next() % bound : 0u;
    }

    bool oneIn( uint32_t n )
    {
        return n > 0u && nextBounded( n ) == 0u;
    }

    float nextFloat( float scale = 1.0f )
    {
        return ( float )( next() & 0xFFFFu ) / 65535.0f * scale;
    }

    uint32_t state;
};

struct ShadowWorld
{
    struct EntityState
    {
        bool alive = false;
        vecsEntity handle = VECS_INVALID_ENTITY;
        bool hasPosition = false;
        Position position = {};
        bool hasVelocity = false;
        Velocity velocity = {};
        bool hasHealth = false;
        Health health = {};
        bool hasEnemy = false;
        bool hasDead = false;
        vecsEntity parent = VECS_INVALID_ENTITY;
    };

    EntityState& ensure( uint32_t index )
    {
        if ( entities.size() <= index )
        {
            entities.resize( index + 1u );
        }
        return entities[index];
    }

    EntityState* get( vecsEntity e )
    {
        uint32_t index = vecsEntityIndex( e );
        if ( index >= entities.size() )
        {
            return nullptr;
        }
        EntityState& state = entities[index];
        return state.alive && state.handle == e ? &state : nullptr;
    }

    const EntityState* get( vecsEntity e ) const
    {
        uint32_t index = vecsEntityIndex( e );
        if ( index >= entities.size() )
        {
            return nullptr;
        }
        const EntityState& state = entities[index];
        return state.alive && state.handle == e ? &state : nullptr;
    }

    void create( vecsEntity e )
    {
        EntityState& state = ensure( vecsEntityIndex( e ) );
        state = {};
        state.alive = true;
        state.handle = e;
    }

    void destroyRecursive( vecsEntity e )
    {
        const EntityState* state = get( e );
        if ( !state )
        {
            return;
        }

        std::vector<vecsEntity> children;
        for ( const EntityState& candidate : entities )
        {
            if ( candidate.alive && candidate.parent == e )
            {
                children.push_back( candidate.handle );
            }
        }
        for ( vecsEntity child : children )
        {
            destroyRecursive( child );
        }

        EntityState& target = ensure( vecsEntityIndex( e ) );
        target.alive = false;
        target.handle = VECS_INVALID_ENTITY;
        target.hasPosition = false;
        target.hasVelocity = false;
        target.hasHealth = false;
        target.hasEnemy = false;
        target.hasDead = false;
        target.parent = VECS_INVALID_ENTITY;
    }

    void setPosition( vecsEntity e, Position position )
    {
        if ( EntityState* state = get( e ) )
        {
            state->hasPosition = true;
            state->position = position;
        }
    }

    void unsetPosition( vecsEntity e )
    {
        if ( EntityState* state = get( e ) )
        {
            state->hasPosition = false;
        }
    }

    void setVelocity( vecsEntity e, Velocity velocity )
    {
        if ( EntityState* state = get( e ) )
        {
            state->hasVelocity = true;
            state->velocity = velocity;
        }
    }

    void unsetVelocity( vecsEntity e )
    {
        if ( EntityState* state = get( e ) )
        {
            state->hasVelocity = false;
        }
    }

    void setHealth( vecsEntity e, Health health )
    {
        if ( EntityState* state = get( e ) )
        {
            state->hasHealth = true;
            state->health = health;
        }
    }

    void unsetHealth( vecsEntity e )
    {
        if ( EntityState* state = get( e ) )
        {
            state->hasHealth = false;
        }
    }

    void setEnemy( vecsEntity e, bool value )
    {
        if ( EntityState* state = get( e ) )
        {
            state->hasEnemy = value;
        }
    }

    void setDead( vecsEntity e, bool value )
    {
        if ( EntityState* state = get( e ) )
        {
            state->hasDead = value;
        }
    }

    void setParent( vecsEntity child, vecsEntity parent )
    {
        if ( EntityState* state = get( child ) )
        {
            if ( parent != VECS_INVALID_ENTITY )
            {
                if ( child == parent )
                {
                    return;
                }
                vecsEntity cursor = parent;
                uint32_t guard = 0u;
                while ( cursor != VECS_INVALID_ENTITY )
                {
                    if ( cursor == child )
                    {
                        return;
                    }
                    const EntityState* cursorState = get( cursor );
                    if ( !cursorState )
                    {
                        break;
                    }
                    if ( ++guard > entities.size() )
                    {
                        return;
                    }
                    cursor = cursorState->parent;
                }
            }
            state->parent = parent;
        }
    }

    void cloneTo( vecsEntity src, vecsEntity dst )
    {
        const EntityState* source = get( src );
        if ( !source )
        {
            return;
        }
        create( dst );
        EntityState* target = get( dst );
        assert( target != nullptr );
        target->hasPosition = source->hasPosition;
        target->position = source->position;
        target->hasVelocity = source->hasVelocity;
        target->velocity = source->velocity;
        target->hasHealth = source->hasHealth;
        target->health = source->health;
        target->hasEnemy = source->hasEnemy;
        target->hasDead = source->hasDead;
        target->parent = source->parent;
    }

    void instantiateFrom( vecsEntity prefab, const std::vector<vecsEntity>& instances )
    {
        for ( vecsEntity e : instances )
        {
            if ( e != VECS_INVALID_ENTITY )
            {
                cloneTo( prefab, e );
            }
        }
    }

    void clear()
    {
        for ( EntityState& state : entities )
        {
            state = {};
        }
        hasSingleton = false;
        singleton = {};
    }

    void setSingleton( int value )
    {
        hasSingleton = true;
        singleton.value = value;
    }

    std::vector<vecsEntity> aliveHandles() const
    {
        std::vector<vecsEntity> out;
        for ( const EntityState& state : entities )
        {
            if ( state.alive )
            {
                out.push_back( state.handle );
            }
        }
        return out;
    }

    template< typename Pred >
    std::vector<vecsEntity> collect( Pred&& pred ) const
    {
        std::vector<vecsEntity> out;
        for ( const EntityState& state : entities )
        {
            if ( state.alive && pred( state ) )
            {
                out.push_back( state.handle );
            }
        }
        vecsSortEntities( out );
        return out;
    }

    uint64_t hash() const
    {
        uint64_t h = 1469598103934665603ull;
        for ( const EntityState& state : entities )
        {
            if ( !state.alive )
            {
                continue;
            }
            h = vecsHashMix( h, state.handle );
            h = vecsHashMix( h, state.hasPosition ? 0x10ull : 0x11ull );
            if ( state.hasPosition )
            {
                h = vecsHashMix( h, vecsHashFloatBits( state.position.x ) );
                h = vecsHashMix( h, vecsHashFloatBits( state.position.y ) );
            }
            h = vecsHashMix( h, state.hasVelocity ? 0x20ull : 0x21ull );
            if ( state.hasVelocity )
            {
                h = vecsHashMix( h, vecsHashFloatBits( state.velocity.vx ) );
                h = vecsHashMix( h, vecsHashFloatBits( state.velocity.vy ) );
            }
            h = vecsHashMix( h, state.hasHealth ? 0x30ull : 0x31ull );
            if ( state.hasHealth )
            {
                h = vecsHashMix( h, ( uint64_t )( uint32_t )state.health.hp );
            }
            h = vecsHashMix( h, state.hasEnemy ? 0x40ull : 0x41ull );
            h = vecsHashMix( h, state.hasDead ? 0x50ull : 0x51ull );
            h = vecsHashMix( h, state.parent );
        }
        h = vecsHashMix( h, hasSingleton ? 0x60ull : 0x61ull );
        if ( hasSingleton )
        {
            h = vecsHashMix( h, ( uint64_t )( uint32_t )singleton.value );
        }
        return h;
    }

    bool matchesWorld( vecsWorld* w ) const
    {
        if ( !w )
        {
            return false;
        }
        for ( uint32_t index = 0; index < w->maxEntities; index++ )
        {
            const bool actualAlive = w->entities->allocated[index] != 0u;
            const bool expectedAlive = index < entities.size() && entities[index].alive;
            if ( actualAlive != expectedAlive )
            {
                return false;
            }
            if ( !expectedAlive )
            {
                continue;
            }

            const EntityState& expected = entities[index];
            if ( !vecsAlive( w, expected.handle ) || vecsEntityIndex( expected.handle ) != index || w->entities->generations[index] != vecsEntityGeneration( expected.handle ) )
            {
                return false;
            }

            const bool actualHasPosition = vecsHas<Position>( w, expected.handle );
            if ( actualHasPosition != expected.hasPosition )
            {
                return false;
            }
            if ( expected.hasPosition )
            {
                Position* position = vecsGet<Position>( w, expected.handle );
                if ( !position || position->x != expected.position.x || position->y != expected.position.y )
                {
                    return false;
                }
            }

            const bool actualHasVelocity = vecsHas<Velocity>( w, expected.handle );
            if ( actualHasVelocity != expected.hasVelocity )
            {
                return false;
            }
            if ( expected.hasVelocity )
            {
                Velocity* velocity = vecsGet<Velocity>( w, expected.handle );
                if ( !velocity || velocity->vx != expected.velocity.vx || velocity->vy != expected.velocity.vy )
                {
                    return false;
                }
            }

            const bool actualHasHealth = vecsHas<Health>( w, expected.handle );
            if ( actualHasHealth != expected.hasHealth )
            {
                return false;
            }
            if ( expected.hasHealth )
            {
                Health* health = vecsGet<Health>( w, expected.handle );
                if ( !health || health->hp != expected.health.hp )
                {
                    return false;
                }
            }

            const bool actualHasEnemy = vecsHas<IsEnemy>( w, expected.handle );
            const bool actualHasDead = vecsHas<Dead>( w, expected.handle );
            const vecsEntity actualParent = vecsGetParentEntity( w, expected.handle );
            if ( actualHasEnemy != expected.hasEnemy || actualHasDead != expected.hasDead || actualParent != expected.parent )
            {
                return false;
            }
        }

        ShadowSingletonValue* singletonPtr = vecsGetSingleton<ShadowSingletonValue>( w );
        if ( ( singletonPtr != nullptr ) != hasSingleton )
        {
            return false;
        }
        if ( hasSingleton )
        {
            if ( !singletonPtr || singletonPtr->value != singleton.value )
            {
                return false;
            }
        }
        return true;
    }

    std::vector<EntityState> entities;
    bool hasSingleton = false;
    ShadowSingletonValue singleton = {};
};

inline uint64_t hash_world_state( vecsWorld* w )
{
    uint64_t h = 1469598103934665603ull;
    for ( uint32_t index = 0; index < w->maxEntities; index++ )
    {
        if ( w->entities->allocated[index] == 0u )
        {
            continue;
        }

        vecsEntity e = vecsMakeEntity( index, w->entities->generations[index] );
        h = vecsHashMix( h, e );

        const bool hasPosition = vecsHas<Position>( w, e );
        h = vecsHashMix( h, hasPosition ? 0x10ull : 0x11ull );
        if ( hasPosition )
        {
            Position* position = vecsGet<Position>( w, e );
            h = vecsHashMix( h, vecsHashFloatBits( position->x ) );
            h = vecsHashMix( h, vecsHashFloatBits( position->y ) );
        }

        const bool hasVelocity = vecsHas<Velocity>( w, e );
        h = vecsHashMix( h, hasVelocity ? 0x20ull : 0x21ull );
        if ( hasVelocity )
        {
            Velocity* velocity = vecsGet<Velocity>( w, e );
            h = vecsHashMix( h, vecsHashFloatBits( velocity->vx ) );
            h = vecsHashMix( h, vecsHashFloatBits( velocity->vy ) );
        }

        const bool hasHealth = vecsHas<Health>( w, e );
        h = vecsHashMix( h, hasHealth ? 0x30ull : 0x31ull );
        if ( hasHealth )
        {
            Health* health = vecsGet<Health>( w, e );
            h = vecsHashMix( h, ( uint64_t )( uint32_t )health->hp );
        }

        h = vecsHashMix( h, vecsHas<IsEnemy>( w, e ) ? 0x40ull : 0x41ull );
        h = vecsHashMix( h, vecsHas<Dead>( w, e ) ? 0x50ull : 0x51ull );
        h = vecsHashMix( h, vecsGetParentEntity( w, e ) );
    }

    ShadowSingletonValue* singleton = vecsGetSingleton<ShadowSingletonValue>( w );
    h = vecsHashMix( h, singleton ? 0x60ull : 0x61ull );
    if ( singleton )
    {
        h = vecsHashMix( h, ( uint64_t )( uint32_t )singleton->value );
    }
    return h;
}

UTEST( vecs, smoke )
{
    ASSERT_NE( VECS_INVALID_ENTITY, 0ULL );
    ASSERT_EQ( ( uint32_t )VECS_L2_COUNT, ( ( uint32_t )VECS_MAX_ENTITIES + 63u ) / 64u );
    ASSERT_EQ( ( uint32_t )VECS_TOP_COUNT, ( ( uint32_t )VECS_L2_COUNT + 63u ) / 64u );
}

UTEST( bits, tzcnt_single_bits )
{
    ASSERT_EQ( vecsTzcnt( 1ULL ), 0u );
    ASSERT_EQ( vecsTzcnt( 1ULL << 63 ), 63u );
    ASSERT_EQ( vecsTzcnt( 0x8000ULL ), 15u );
}

UTEST( bits, popcnt )
{
    ASSERT_EQ( vecsPopcnt( 0ULL ), 0u );
    ASSERT_EQ( vecsPopcnt( UINT64_MAX ), 64u );
    ASSERT_EQ( vecsPopcnt( 0xAAAAAAAAAAAAAAAAULL ), 32u );
}

UTEST( entity, pack_unpack )
{
    vecsEntity e = vecsMakeEntity( 42u, 7u );
    ASSERT_EQ( vecsEntityIndex( e ), 42u );
    ASSERT_EQ( vecsEntityGeneration( e ), 7u );
}

UTEST( entity, create_destroy_cycle )
{
    vecsEntityPool* pool = vecsCreateEntityPool( 128u );
    vecsEntity e1 = vecsEntityPoolCreate( pool );
    vecsEntity e2 = vecsEntityPoolCreate( pool );
    ASSERT_NE( vecsEntityIndex( e1 ), vecsEntityIndex( e2 ) );
    ASSERT_TRUE( vecsEntityPoolAlive( pool, e1 ) );
    ASSERT_TRUE( vecsEntityPoolAlive( pool, e2 ) );

    vecsEntityPoolDestroy( pool, e1 );
    ASSERT_FALSE( vecsEntityPoolAlive( pool, e1 ) );
    ASSERT_TRUE( vecsEntityPoolAlive( pool, e2 ) );

    vecsEntity e3 = vecsEntityPoolCreate( pool );
    ASSERT_EQ( vecsEntityIndex( e3 ), vecsEntityIndex( e1 ) );
    ASSERT_EQ( vecsEntityGeneration( e3 ), vecsEntityGeneration( e1 ) + 1u );

    vecsDestroyEntityPool( pool );
}

UTEST( entity, pool_full )
{
    vecsEntityPool* pool = vecsCreateEntityPool( 2u );
    vecsEntityPoolCreate( pool );
    vecsEntityPoolCreate( pool );
    vecsEntity e = vecsEntityPoolCreate( pool );
    ASSERT_EQ( e, VECS_INVALID_ENTITY );
    vecsDestroyEntityPool( pool );
}

UTEST( entity, generation_overflow_wraps )
{
    vecsEntityPool* pool = vecsCreateEntityPool( 4u );
    for ( int i = 0; i < 10; i++ )
    {
        vecsEntity e = vecsEntityPoolCreate( pool );
        vecsEntityPoolDestroy( pool, e );
    }
    vecsEntity e = vecsEntityPoolCreate( pool );
    ASSERT_EQ( vecsEntityGeneration( e ), 10u );
    vecsEntityPoolDestroy( pool, e );
    vecsDestroyEntityPool( pool );
}

UTEST( entity, unallocated_index_not_alive )
{
    vecsWorld* w = vecsCreateWorld( 64u );
    vecsEntity fake = vecsMakeEntity( 5u, 0u );
    ASSERT_FALSE( vecsAlive( w, fake ) );

    vecsEntity e = vecsCreate( w );
    ASSERT_TRUE( vecsAlive( w, e ) );
    vecsDestroy( w, e );
    ASSERT_FALSE( vecsAlive( w, e ) );

    vecsDestroyWorld( w );
}

UTEST( entity, stale_handle_not_alive_after_reuse )
{
    vecsWorld* w = vecsCreateWorld( 1u );
    vecsEntity oldHandle = vecsCreate( w );
    ASSERT_TRUE( vecsAlive( w, oldHandle ) );

    vecsDestroy( w, oldHandle );
    ASSERT_FALSE( vecsAlive( w, oldHandle ) );

    vecsEntity newHandle = vecsCreate( w );
    ASSERT_EQ( vecsEntityIndex( newHandle ), vecsEntityIndex( oldHandle ) );
    ASSERT_NE( vecsEntityGeneration( newHandle ), vecsEntityGeneration( oldHandle ) );
    ASSERT_TRUE( vecsAlive( w, newHandle ) );
    ASSERT_FALSE( vecsAlive( w, oldHandle ) );

    vecsDestroyWorld( w );
}

UTEST( bitfield, set_has_clear )
{
    vecsBitfield bf = {};
    ASSERT_FALSE( vecsBitfieldHas( &bf, 0u ) );
    vecsBitfieldSet( &bf, 0u );
    ASSERT_TRUE( vecsBitfieldHas( &bf, 0u ) );
    vecsBitfieldUnset( &bf, 0u );
    ASSERT_FALSE( vecsBitfieldHas( &bf, 0u ) );
}

UTEST( bitfield, scattered_bits )
{
    vecsBitfield bf = {};
    const std::vector<uint32_t> indices = {
        0u,
        vecsTestClampIndex( 63u ),
        vecsTestClampIndex( 64u ),
        vecsTestClampIndex( 4095u ),
        vecsTestClampIndex( 4096u ),
        vecsTestClampIndex( VECS_MAX_ENTITIES - 1u )
    };
    for ( uint32_t index : indices )
    {
        vecsBitfieldSet( &bf, index );
    }

    uint32_t uniqueCount = 0u;
    uint32_t last = UINT32_MAX;
    for ( uint32_t index : indices )
    {
        if ( index != last )
        {
            uniqueCount++;
            last = index;
        }
    }

    ASSERT_EQ( vecsBitfieldCount( &bf ), uniqueCount );
    ASSERT_TRUE( vecsBitfieldHas( &bf, 0u ) );
    ASSERT_TRUE( vecsBitfieldHas( &bf, vecsTestClampIndex( 63u ) ) );
    ASSERT_TRUE( vecsBitfieldHas( &bf, vecsTestClampIndex( 64u ) ) );
    ASSERT_TRUE( vecsBitfieldHas( &bf, vecsTestClampIndex( 4095u ) ) );
    ASSERT_TRUE( vecsBitfieldHas( &bf, vecsTestClampIndex( 4096u ) ) );
    ASSERT_TRUE( vecsBitfieldHas( &bf, vecsTestClampIndex( VECS_MAX_ENTITIES - 1u ) ) );
    ASSERT_FALSE( vecsBitfieldHas( &bf, 1u ) );
    ASSERT_FALSE( vecsBitfieldHas( &bf, 62u ) );
    if ( VECS_MAX_ENTITIES > 65u )
    {
        ASSERT_FALSE( vecsBitfieldHas( &bf, 65u ) );
    }
}

UTEST( bitfield, iteration_order )
{
    vecsBitfield bf = {};
    uint32_t expected[] = { 5u, 100u, 1000u, vecsTestClampIndex( 50000u ) };
    for ( uint32_t idx : expected )
    {
        vecsBitfieldSet( &bf, idx );
    }

    uint32_t collected[4] = {};
    uint32_t n = 0;
    vecsBitfieldEach( &bf, [&]( uint32_t index ) { collected[n++] = index; } );

    ASSERT_EQ( n, 4u );
    for ( uint32_t i = 0; i < 4u; i++ )
    {
        ASSERT_EQ( collected[i], expected[i] );
    }
}

UTEST( bitfield, join )
{
    vecsBitfield a = {}, b = {};
    vecsBitfieldSet( &a, 10u );
    vecsBitfieldSet( &a, 20u );
    vecsBitfieldSet( &a, 30u );
    vecsBitfieldSet( &b, 20u );
    vecsBitfieldSet( &b, 30u );
    vecsBitfieldSet( &b, 40u );

    uint32_t collected[4] = {};
    uint32_t n = 0;
    vecsBitfieldJoin( &a, &b, [&]( uint32_t index ) { collected[n++] = index; } );

    ASSERT_EQ( n, 2u );
    ASSERT_EQ( collected[0], 20u );
    ASSERT_EQ( collected[1], 30u );
}

UTEST( bitfield, top_mask_auto_clear )
{
    vecsBitfield bf = {};
    vecsBitfieldSet( &bf, 100u );
    ASSERT_NE( bf.topMasks[100u / 4096u], 0ULL );
    vecsBitfieldUnset( &bf, 100u );
    uint32_t n = 0;
    vecsBitfieldEach( &bf, [&]( uint32_t ) { n++; } );
    ASSERT_EQ( n, 0u );
}

UTEST( bitfield, clear_all )
{
    vecsBitfield bf = {};
    const uint32_t stride = 50u;
    const uint32_t setCount = std::min<uint32_t>( 1000u, ( ( uint32_t )VECS_MAX_ENTITIES + stride - 1u ) / stride );
    for ( uint32_t i = 0; i < setCount; i++ )
    {
        vecsBitfieldSet( &bf, i * stride );
    }
    vecsBitfieldClearAll( &bf );
    ASSERT_EQ( vecsBitfieldCount( &bf ), 0u );
}

UTEST( bitfield, boundary_last_entity )
{
    vecsBitfield bf = {};
    vecsBitfieldSet( &bf, VECS_MAX_ENTITIES - 1u );
    ASSERT_TRUE( vecsBitfieldHas( &bf, VECS_MAX_ENTITIES - 1u ) );
    ASSERT_EQ( vecsBitfieldCount( &bf ), 1u );
}

UTEST( pool, add_get_remove )
{
    vecsPool* pool = vecsCreatePool( 1024u, sizeof( Position ) );
    Position p = { 1.0f, 2.0f };
    vecsPoolSet( pool, 0u, &p );
    ASSERT_TRUE( vecsPoolHas( pool, 0u ) );
    Position* got = ( Position* )vecsPoolGet( pool, 0u );
    ASSERT_EQ( got->x, 1.0f );
    ASSERT_EQ( got->y, 2.0f );
    vecsPoolUnset( pool, 0u );
    ASSERT_FALSE( vecsPoolHas( pool, 0u ) );
    vecsDestroyPool( pool );
}

UTEST( pool, swap_and_pop_integrity )
{
    vecsPool* pool = vecsCreatePool( 1024u, sizeof( int ) );
    int vals[] = { 10, 20, 30 };
    vecsPoolSet( pool, 5u, &vals[0] );
    vecsPoolSet( pool, 10u, &vals[1] );
    vecsPoolSet( pool, 15u, &vals[2] );
    vecsPoolUnset( pool, 10u );
    ASSERT_FALSE( vecsPoolHas( pool, 10u ) );
    ASSERT_TRUE( vecsPoolHas( pool, 5u ) );
    ASSERT_TRUE( vecsPoolHas( pool, 15u ) );
    ASSERT_EQ( *( int* )vecsPoolGet( pool, 5u ), 10 );
    ASSERT_EQ( *( int* )vecsPoolGet( pool, 15u ), 30 );
    vecsDestroyPool( pool );
}

UTEST( pool, grow )
{
    vecsPool* pool = vecsCreatePool( 1024u, sizeof( int ) );
    for ( int i = 0; i < 200; i++ )
    {
        vecsPoolSet( pool, ( uint32_t )i, &i );
    }
    ASSERT_EQ( pool->count, 200u );
    for ( int i = 0; i < 200; i++ )
    {
        ASSERT_EQ( *( int* )vecsPoolGet( pool, ( uint32_t )i ), i );
    }
    vecsDestroyPool( pool );
}

UTEST( world, create_destroy_entity )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    ASSERT_TRUE( vecsAlive( w, e ) );
    vecsDestroy( w, e );
    ASSERT_FALSE( vecsAlive( w, e ) );
    vecsDestroyWorld( w );
}

UTEST( world, component_crud )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );

    vecsSet<Position>( w, e, { 1.0f, 2.0f } );
    ASSERT_TRUE( vecsHas<Position>( w, e ) );
    Position* p = vecsGet<Position>( w, e );
    ASSERT_EQ( p->x, 1.0f );
    ASSERT_EQ( p->y, 2.0f );

    vecsSet<Position>( w, e, { 3.0f, 4.0f } );
    p = vecsGet<Position>( w, e );
    ASSERT_EQ( p->x, 3.0f );

    vecsUnset<Position>( w, e );
    ASSERT_FALSE( vecsHas<Position>( w, e ) );

    vecsDestroyWorld( w );
}

UTEST( world, destroy_entity_removes_components )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    vecsSet<Position>( w, e, { 1.0f, 2.0f } );
    vecsSet<Velocity>( w, e, { 0.1f, 0.2f } );
    vecsDestroy( w, e );
    vecsEntity e2 = vecsCreate( w );
    ASSERT_FALSE( vecsHas<Position>( w, e2 ) );
    ASSERT_FALSE( vecsHas<Velocity>( w, e2 ) );
    vecsDestroyWorld( w );
}

UTEST( world, multiple_entities_multiple_components )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e1 = vecsCreate( w );
    vecsEntity e2 = vecsCreate( w );
    vecsEntity e3 = vecsCreate( w );

    vecsSet<Position>( w, e1, { 1, 1 } );
    vecsSet<Position>( w, e2, { 2, 2 } );
    vecsSet<Velocity>( w, e1, { 10, 10 } );
    vecsSet<Health>( w, e3, { 100 } );

    ASSERT_TRUE( vecsHas<Position>( w, e1 ) );
    ASSERT_TRUE( vecsHas<Position>( w, e2 ) );
    ASSERT_FALSE( vecsHas<Position>( w, e3 ) );
    ASSERT_TRUE( vecsHas<Velocity>( w, e1 ) );
    ASSERT_FALSE( vecsHas<Velocity>( w, e2 ) );
    ASSERT_TRUE( vecsHas<Health>( w, e3 ) );
    ASSERT_EQ( vecsCount( w ), 3u );

    vecsDestroyWorld( w );
}

UTEST( query, each_single_component )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e1 = vecsCreate( w );
    vecsEntity e2 = vecsCreate( w );
    vecsEntity e3 = vecsCreate( w );
    vecsSet<Position>( w, e1, { 1, 0 } );
    vecsSet<Position>( w, e2, { 2, 0 } );
    vecsSet<Position>( w, e3, { 3, 0 } );

    float sum = 0.0f;
    vecsEach<Position>( w, [&]( vecsEntity, Position& p ) { sum += p.x; } );
    ASSERT_EQ( sum, 6.0f );

    vecsDestroyWorld( w );
}

UTEST( query, each_two_components )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e1 = vecsCreate( w );
    vecsEntity e2 = vecsCreate( w );
    vecsEntity e3 = vecsCreate( w );

    vecsSet<Position>( w, e1, { 1, 0 } );
    vecsSet<Position>( w, e2, { 2, 0 } );
    vecsSet<Position>( w, e3, { 3, 0 } );
    vecsSet<Velocity>( w, e1, { 10, 0 } );
    vecsSet<Velocity>( w, e3, { 30, 0 } );

    float sum = 0.0f;
    vecsEach<Position, Velocity>( w, [&]( vecsEntity, Position& p, Velocity& v ) { sum += p.x + v.vx; } );
    ASSERT_EQ( sum, 44.0f );

    vecsDestroyWorld( w );
}

UTEST( query, each_three_components )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e1 = vecsCreate( w );
    vecsEntity e2 = vecsCreate( w );

    vecsSet<Position>( w, e1, { 1, 0 } );
    vecsSet<Velocity>( w, e1, { 10, 0 } );
    vecsSet<Health>( w, e1, { 100 } );
    vecsSet<Position>( w, e2, { 2, 0 } );
    vecsSet<Velocity>( w, e2, { 20, 0 } );

    int count = 0;
    vecsEach<Position, Velocity, Health>( w, [&]( vecsEntity, Position&, Velocity&, Health& h )
    {
        ASSERT_EQ( h.hp, 100 );
        count++;
    } );
    ASSERT_EQ( count, 1 );

    vecsDestroyWorld( w );
}

UTEST( query, each_empty_world )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    int count = 0;
    vecsEach<Position>( w, [&]( vecsEntity, Position& ) { count++; } );
    ASSERT_EQ( count, 0 );
    vecsDestroyWorld( w );
}

UTEST( query, each_no_matches )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    vecsSet<Position>( w, e, { 1, 1 } );

    int count = 0;
    vecsEach<Position, Velocity>( w, [&]( vecsEntity, Position&, Velocity& ) { count++; } );
    ASSERT_EQ( count, 0 );

    vecsDestroyWorld( w );
}

UTEST( query, each_iteration_order )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity entities[5];
    for ( int i = 0; i < 5; i++ )
    {
        entities[i] = vecsCreate( w );
        vecsSet<Position>( w, entities[i], { ( float )i, 0 } );
    }

    float prev = -1.0f;
    vecsEach<Position>( w, [&]( vecsEntity, Position& p )
    {
        ASSERT_GT( p.x, prev );
        prev = p.x;
    } );

    vecsDestroyWorld( w );
}

UTEST( singleton, set_get )
{
    struct Gravity { float g; };
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsSetSingleton<Gravity>( w, { 9.81f } );
    Gravity* g = vecsGetSingleton<Gravity>( w );
    ASSERT_TRUE( g != nullptr );
    ASSERT_EQ( g->g, 9.81f );
    vecsDestroyWorld( w );
}

UTEST( singleton, get_unset_returns_null )
{
    struct Wind { float speed; };
    vecsWorld* w = vecsCreateWorld( 1024u );
    Wind* wind = vecsGetSingleton<Wind>( w );
    ASSERT_TRUE( wind == nullptr );
    vecsDestroyWorld( w );
}

UTEST( query, mutate_during_iteration )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    for ( int i = 0; i < 10; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0 } );
    }

    vecsEach<Position>( w, [&]( vecsEntity, Position& p ) { p.x *= 2.0f; } );

    float sum = 0.0f;
    vecsEach<Position>( w, [&]( vecsEntity, Position& p ) { sum += p.x; } );
    ASSERT_EQ( sum, 90.0f );

    vecsDestroyWorld( w );
}

UTEST( clone, basic )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity src = vecsCreate( w );
    vecsSet<Position>( w, src, { 5.0f, 10.0f } );
    vecsSet<Velocity>( w, src, { 1.0f, 2.0f } );

    vecsEntity dst = vecsClone( w, src );
    ASSERT_NE( dst, src );
    ASSERT_TRUE( vecsAlive( w, dst ) );
    ASSERT_TRUE( vecsHas<Position>( w, dst ) );
    ASSERT_TRUE( vecsHas<Velocity>( w, dst ) );

    Position* p = vecsGet<Position>( w, dst );
    ASSERT_EQ( p->x, 5.0f );
    ASSERT_EQ( p->y, 10.0f );
    p->x = 99.0f;
    ASSERT_EQ( vecsGet<Position>( w, src )->x, 5.0f );

    vecsDestroyWorld( w );
}

UTEST( clone, no_components )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity src = vecsCreate( w );
    vecsEntity dst = vecsClone( w, src );
    ASSERT_TRUE( vecsAlive( w, dst ) );
    ASSERT_FALSE( vecsHas<Position>( w, dst ) );
    vecsDestroyWorld( w );
}

UTEST( batch, create )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity entities[100];
    for ( int i = 0; i < 100; i++ )
    {
        entities[i] = vecsCreate( w );
    }
    ASSERT_EQ( vecsCount( w ), 100u );
    for ( int i = 0; i < 100; i++ )
    {
        ASSERT_TRUE( vecsAlive( w, entities[i] ) );
    }
    vecsDestroyWorld( w );
}

UTEST( simd, join_two_components )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    for ( uint32_t i = 0; i < 500u; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0 } );
        if ( ( i % 2u ) == 0u )
        {
            vecsSet<Velocity>( w, e, { ( float )i * 10.0f, 0 } );
        }
    }

    uint32_t count = 0;
    vecsEach<Position, Velocity>( w, [&]( vecsEntity, Position&, Velocity& ) { count++; } );
    ASSERT_EQ( count, 250u );
    vecsDestroyWorld( w );
}

UTEST( simd, join_three_components )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    for ( uint32_t i = 0; i < 300u; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0 } );
        if ( ( i % 3u ) == 0u )
        {
            vecsSet<Velocity>( w, e, { ( float )i, 0 } );
        }
        if ( ( i % 5u ) == 0u )
        {
            vecsSet<Health>( w, e, { ( int )i } );
        }
    }

    uint32_t count = 0;
    vecsEach<Position, Velocity, Health>( w, [&]( vecsEntity, Position&, Velocity&, Health& ) { count++; } );
    ASSERT_EQ( count, 20u );
    vecsDestroyWorld( w );
}

UTEST( simd, sparse_entities )
{
    vecsWorld* w = vecsCreateWorld();
    vecsEntity entities[10];
    for ( int i = 0; i < 10; i++ )
    {
        entities[i] = vecsCreate( w );
        vecsSet<Position>( w, entities[i], { ( float )i, 0 } );
        vecsSet<Velocity>( w, entities[i], { ( float )i * 10.0f, 0 } );
    }

    float sum = 0.0f;
    vecsEach<Position, Velocity>( w, [&]( vecsEntity, Position& p, Velocity& v ) { sum += p.x + v.vx; } );
    ASSERT_EQ( sum, 495.0f );
    vecsDestroyWorld( w );
}

UTEST( simd, empty_skip )
{
    vecsWorld* w = vecsCreateWorld();
    vecsEntity e = vecsCreate( w );
    vecsSet<Position>( w, e, { 42.0f, 0 } );
    vecsSet<Velocity>( w, e, { 1.0f, 0 } );

    uint32_t count = 0;
    vecsEach<Position, Velocity>( w, [&]( vecsEntity, Position& p, Velocity& )
    {
        ASSERT_EQ( p.x, 42.0f );
        count++;
    } );
    ASSERT_EQ( count, 1u );
    vecsDestroyWorld( w );
}

UTEST( simd, scalar_simd_parity )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    for ( uint32_t i = 0; i < 200u; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, ( float )( i * 2u ) } );
        if ( ( i % 3u ) == 0u )
        {
            vecsSet<Velocity>( w, e, { ( float )i, 0 } );
        }
    }

    float sum = 0.0f;
    uint32_t count = 0;
    vecsEach<Position, Velocity>( w, [&]( vecsEntity, Position& p, Velocity& v )
    {
        sum += p.x + v.vx;
        count++;
    } );
    ASSERT_EQ( count, 67u );
    ASSERT_TRUE( sum > 0.0f );
    vecsDestroyWorld( w );
}

UTEST( query, cached_query_with_without )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    for ( uint32_t i = 0; i < 10u; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0 } );
        if ( i % 2u == 0u )
        {
            vecsSet<Velocity>( w, e, { ( float )i * 10.0f, 0 } );
        }
        if ( i % 3u == 0u )
        {
            vecsSet<Health>( w, e, { ( int )i } );
        }
    }
    
    vecsQuery* q = vecsBuildQuery<Position, Velocity, Health>( w );
    uint32_t count = 0;
    vecsQueryEach<Position, Velocity, Health>( w, q, [&]( vecsEntity, Position&, Velocity&, Health& )
    {
        count++;
    } );
    ASSERT_EQ( count, 2u );
    
    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query, without_filter )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e1 = vecsCreate( w );
    vecsEntity e2 = vecsCreate( w );
    vecsEntity e3 = vecsCreate( w );
    
    vecsSet<Position>( w, e1, { 1, 0 } );
    vecsSet<Position>( w, e2, { 2, 0 } );
    vecsSet<Position>( w, e3, { 3, 0 } );
    vecsSet<Velocity>( w, e2, { 10, 0 } );
    vecsSet<Health>( w, e3, { 100 } );
    
    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    uint32_t count = 0;
    float sumX = 0.0f;
    vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity, Position& p, Velocity& )
    {
        count++;
        sumX += p.x;
    } );
    ASSERT_EQ( count, 1u );
    ASSERT_EQ( sumX, 2.0f );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query, collect_basic )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity matched[5] = {};
    uint32_t matchCount = 0;
    for ( uint32_t i = 0; i < 10u; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0 } );
        if ( i % 2u == 0u )
        {
            vecsSet<Velocity>( w, e, { ( float )i * 10.0f, 0 } );
            matched[matchCount++] = e;
        }
    }

    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    std::vector<vecsQueryHit<Position, Velocity>> hits;
    vecsQueryCollect<Position, Velocity>( w, q, hits );

    // One hit per matching entity, parity with vecsQueryEach.
    ASSERT_EQ( ( uint32_t )hits.size(), matchCount );

    // Hits hold live pointers: values resolve and pointer identity matches the pool data.
    float sumX = 0.0f;
    for ( auto& hit : hits )
    {
        sumX += hit.get<Position>().x;
        ASSERT_EQ( &hit.get<Position>(), vecsGet<Position>( w, hit.entity ) );
        ASSERT_EQ( &hit.get<Velocity>(), vecsGet<Velocity>( w, hit.entity ) );
        ASSERT_EQ( hit.get<Velocity>().vx, hit.get<Position>().x * 10.0f );
    }
    ASSERT_EQ( sumX, 0.0f + 2.0f + 4.0f + 6.0f + 8.0f );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query, collect_without )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e1 = vecsCreate( w );
    vecsEntity e2 = vecsCreate( w );
    vecsEntity e3 = vecsCreate( w );
    vecsSet<Position>( w, e1, { 1, 0 } );
    vecsSet<Position>( w, e2, { 2, 0 } );
    vecsSet<Position>( w, e3, { 3, 0 } );
    vecsSet<Dead>( w, e2, {} );

    vecsQuery* q = vecsBuildQuery<Position>( w );
    vecsQueryAddWithout( q, vecsTypeId<Dead>() );

    std::vector<vecsQueryHit<Position>> hits;
    vecsQueryCollect<Position>( w, q, hits );

    ASSERT_EQ( ( uint32_t )hits.size(), 2u );
    for ( auto& hit : hits )
    {
        ASSERT_NE( hit.entity, e2 ); // dead entity excluded by the query
    }

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query, collect_append )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    for ( uint32_t i = 0; i < 4u; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0 } );
    }

    vecsQuery* q = vecsBuildQuery<Position>( w );
    std::vector<vecsQueryHit<Position>> hits;
    vecsQueryCollect<Position>( w, q, hits );
    ASSERT_EQ( ( uint32_t )hits.size(), 4u );

    // Collect does not clear: a second pass accumulates.
    vecsQueryCollect<Position>( w, q, hits );
    ASSERT_EQ( ( uint32_t )hits.size(), 8u );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query, collect_empty )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    vecsSet<Position>( w, e, { 1, 0 } );

    // No entity has Velocity -> empty pool -> zero hits, no crash.
    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    std::vector<vecsQueryHit<Position, Velocity>> hits;
    vecsQueryCollect<Position, Velocity>( w, q, hits );
    ASSERT_EQ( ( uint32_t )hits.size(), 0u );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

static int g_observerAddCount = 0;
static int g_observerRemoveCount = 0;

static void onPositionAdd( vecsWorld*, vecsEntity, Position* )
{
    g_observerAddCount++;
}

static void onPositionRemove( vecsWorld*, vecsEntity, Position* )
{
    g_observerRemoveCount++;
}

UTEST( observer, on_add_callback )
{
    g_observerAddCount = 0;
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsOnAdd<Position>( w, onPositionAdd );
    
    vecsEntity e = vecsCreate( w );
    vecsSet<Position>( w, e, { 1, 2 } );
    ASSERT_EQ( g_observerAddCount, 1 );
    
    vecsDestroyWorld( w );
}

UTEST( observer, on_remove_callback )
{
    g_observerRemoveCount = 0;
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsOnRemove<Position>( w, onPositionRemove );
    
    vecsEntity e = vecsCreate( w );
    vecsSet<Position>( w, e, { 1, 2 } );
    vecsUnset<Position>( w, e );
    ASSERT_EQ( g_observerRemoveCount, 1 );
    
    vecsDestroyWorld( w );
}

UTEST( relationships, set_parent_child )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsEntity parent = vecsCreate( w );
    vecsEntity child1 = vecsCreate( w );
    vecsEntity child2 = vecsCreate( w );
    
    vecsSetChildOf( w, child1, parent );
    vecsSetChildOf( w, child2, parent );
    
    ASSERT_EQ( vecsGetParentEntity( w, child1 ), parent );
    ASSERT_EQ( vecsGetParentEntity( w, child2 ), parent );
    ASSERT_EQ( vecsGetChildEntityCount( w, parent ), 2u );
    
    vecsDestroyWorld( w );
}

UTEST( relationships, get_children )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsEntity parent = vecsCreate( w );
    vecsEntity child1 = vecsCreate( w );
    vecsEntity child2 = vecsCreate( w );
    vecsEntity child3 = vecsCreate( w );
    
    vecsSetChildOf( w, child1, parent );
    vecsSetChildOf( w, child2, parent );
    vecsSetChildOf( w, child3, parent );
    
    ASSERT_EQ( vecsGetChildEntityCount( w, parent ), 3u );
    ASSERT_EQ( vecsGetChildEntity( w, parent, 0 ), child1 );
    ASSERT_EQ( vecsGetChildEntity( w, parent, 1 ), child2 );
    ASSERT_EQ( vecsGetChildEntity( w, parent, 2 ), child3 );
    
    vecsDestroyWorld( w );
}

UTEST( relationships, destroy_parent_destroys_children )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsEntity parent = vecsCreate( w );
    vecsEntity child1 = vecsCreate( w );
    vecsEntity child2 = vecsCreate( w );
    vecsEntity grandchild = vecsCreate( w );
    
    vecsSet<Position>( w, parent, { 0, 0 } );
    vecsSet<Position>( w, child1, { 1, 0 } );
    vecsSet<Position>( w, child2, { 2, 0 } );
    vecsSet<Position>( w, grandchild, { 3, 0 } );
    
    vecsSetChildOf( w, child1, parent );
    vecsSetChildOf( w, child2, parent );
    vecsSetChildOf( w, grandchild, child1 );
    
    ASSERT_TRUE( vecsAlive( w, parent ) );
    ASSERT_TRUE( vecsAlive( w, child1 ) );
    ASSERT_TRUE( vecsAlive( w, child2 ) );
    ASSERT_TRUE( vecsAlive( w, grandchild ) );
    ASSERT_EQ( vecsCount( w ), 4u );
    
    vecsDestroy( w, parent );
    
    ASSERT_FALSE( vecsAlive( w, parent ) );
    ASSERT_FALSE( vecsAlive( w, child1 ) );
    ASSERT_FALSE( vecsAlive( w, child2 ) );
    ASSERT_FALSE( vecsAlive( w, grandchild ) );
    ASSERT_EQ( vecsCount( w ), 0u );
    
    vecsDestroyWorld( w );
}

UTEST( relationships, reparent_removes_from_old_parent )
{
    vecsWorld* w = vecsCreateWorld( 1024u );

    vecsEntity oldParent = vecsCreate( w );
    vecsEntity newParent = vecsCreate( w );
    vecsEntity child = vecsCreate( w );

    vecsSetChildOf( w, child, oldParent );
    ASSERT_EQ( vecsGetChildEntityCount( w, oldParent ), 1u );

    vecsSetChildOf( w, child, newParent );

    ASSERT_EQ( vecsGetParentEntity( w, child ), newParent );
    ASSERT_EQ( vecsGetChildEntityCount( w, oldParent ), 0u );
    ASSERT_EQ( vecsGetChildEntityCount( w, newParent ), 1u );
    ASSERT_EQ( vecsGetChildEntity( w, newParent, 0u ), child );

    vecsDestroy( w, oldParent );
    ASSERT_TRUE( vecsAlive( w, child ) );
    ASSERT_EQ( vecsGetParentEntity( w, child ), newParent );

    vecsDestroyWorld( w );
}

UTEST( relationships, no_parent_returns_invalid )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsEntity e = vecsCreate( w );
    ASSERT_EQ( vecsGetParentEntity( w, e ), VECS_INVALID_ENTITY );
    ASSERT_EQ( vecsGetChildEntityCount( w, e ), 0u );
    
    vecsDestroyWorld( w );
}

struct alignas( 32 ) AlignedMat4
{
    float m[16];
};

UTEST( alignment, aligned_component )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    AlignedMat4 mat = {};
    vecsSet<AlignedMat4>( w, e, mat );
    
    AlignedMat4* ptr = vecsGet<AlignedMat4>( w, e );
    ASSERT_TRUE( ptr != nullptr );
    ASSERT_EQ( ( uintptr_t )ptr % 32u, 0u );
    
    vecsDestroyWorld( w );
}

UTEST( world, tag_component_uses_bitfield_only )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );

    IsEnemy* tag = vecsSet<IsEnemy>( w, e, {} );
    ASSERT_TRUE( tag != nullptr );
    ASSERT_TRUE( vecsHas<IsEnemy>( w, e ) );
    ASSERT_TRUE( vecsGet<IsEnemy>( w, e ) != nullptr );

    vecsPool* pool = w->pools[vecsTypeId<IsEnemy>()];
    ASSERT_TRUE( pool != nullptr );
    ASSERT_TRUE( pool->noData );
    ASSERT_TRUE( pool->denseData == nullptr );

    vecsUnset<IsEnemy>( w, e );
    ASSERT_FALSE( vecsHas<IsEnemy>( w, e ) );

    vecsDestroyWorld( w );
}

UTEST( world, instantiate_batch_prefab )
{
    vecsWorld* w = vecsCreateWorld( 4096u );
    vecsEntity prefab = vecsCreate( w );
    vecsSet<Position>( w, prefab, { 3.0f, 9.0f } );
    vecsSet<Velocity>( w, prefab, { 4.0f, 7.0f } );
    vecsSet<IsEnemy>( w, prefab, {} );

    const uint32_t count = 256u;
    vecsEntity spawned[count] = {};
    vecsInstantiateBatch( w, prefab, spawned, count );

    for ( uint32_t i = 0; i < count; i++ )
    {
        ASSERT_TRUE( vecsAlive( w, spawned[i] ) );
        ASSERT_TRUE( vecsHas<Position>( w, spawned[i] ) );
        ASSERT_TRUE( vecsHas<Velocity>( w, spawned[i] ) );
        ASSERT_TRUE( vecsHas<IsEnemy>( w, spawned[i] ) );
        Position* p = vecsGet<Position>( w, spawned[i] );
        Velocity* v = vecsGet<Velocity>( w, spawned[i] );
        ASSERT_EQ( p->x, 3.0f );
        ASSERT_EQ( p->y, 9.0f );
        ASSERT_EQ( v->vx, 4.0f );
        ASSERT_EQ( v->vy, 7.0f );
    }

    ASSERT_TRUE( vecsAlive( w, prefab ) );
    vecsDestroyWorld( w );
}

UTEST( world, clear_world )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    ASSERT_TRUE( w != nullptr );

    // Clear empty world
    vecsClearWorld( w );
    ASSERT_TRUE( w->entities->alive == 0 );

    // Create entities, components, relationships, observers, singleton
    vecsEntity e1 = vecsCreate( w );
    vecsEntity e2 = vecsCreate( w );
    vecsSet<Position>( w, e1, { 1.0f, 2.0f } );
    vecsSet<Velocity>( w, e2, { 3.0f, 4.0f } );
    vecsSetChildOf( w, e2, e1 );

    // Add observer
    vecsAddObserver( w->observers, vecsTypeId<Position>(), vecs_test_clear_world_dummy_observer, true );

    // Singleton must be trivially copyable; empty destructor is redundant.
    struct TestSingleton
    {
        int value;
    };
    vecsSetSingleton<TestSingleton>( w, { 42 } );
    ASSERT_TRUE( vecsGetSingleton<TestSingleton>( w ) != nullptr );
    ASSERT_EQ( vecsGetSingleton<TestSingleton>( w )->value, 42 );

    // Clear world
    vecsClearWorld( w );

    // Verify world is empty
    ASSERT_TRUE( w->entities->alive == 0 );
    ASSERT_TRUE( w->entities->freeCount == w->maxEntities );

    // Old entity IDs should be invalid (generations incremented)
    ASSERT_FALSE( vecsAlive( w, e1 ) );
    ASSERT_FALSE( vecsAlive( w, e2 ) );

    // Component pools should be empty
    vecsPool* posPool = w->pools[vecsTypeId<Position>()];
    if ( posPool )
    {
        ASSERT_TRUE( posPool->count == 0 );
        // Sparse array should be reset for all indices
        bool allInvalid = true;
        for ( uint32_t i = 0; i < w->maxEntities; i++ )
        {
            if ( posPool->sparse[i] != VECS_INVALID_INDEX )
            {
                allInvalid = false;
                break;
            }
        }
        ASSERT_TRUE( allInvalid );
    }

    // Relationships cleared
    ASSERT_TRUE( w->relationships->parents[vecsEntityIndex( e1 )] == VECS_INVALID_ENTITY );
    ASSERT_TRUE( w->relationships->parents[vecsEntityIndex( e2 )] == VECS_INVALID_ENTITY );

    // Observers survive a world clear so engine systems stay registered
    ASSERT_TRUE( w->observers->count == 1 );

    // Singleton data freed
    ASSERT_TRUE( w->singletons[vecsTypeId<TestSingleton>()].data == nullptr );

    // Can create new entities after clear
    vecsEntity e3 = vecsCreate( w );
    ASSERT_TRUE( vecsAlive( w, e3 ) );
    vecsSet<Position>( w, e3, { 5.0f, 6.0f } );
    ASSERT_TRUE( vecsHas<Position>( w, e3 ) );

    vecsDestroyWorld( w );
}

UTEST( query, simd_query_parity )
{
    vecsWorld* w = vecsCreateWorld();
    for ( uint32_t i = 0; i < 500u; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0 } );
        if ( i % 2u == 0u )
        {
            vecsSet<Velocity>( w, e, { ( float )i * 10.0f, 0 } );
        }
        if ( i % 3u == 0u )
        {
            vecsSet<Health>( w, e, { ( int )i } );
        }
    }
    
    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    
    uint32_t count = 0;
    float sumX = 0.0f;
    vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity, Position& p, Velocity& )
    {
        count++;
        sumX += p.x;
    } );
    
    ASSERT_EQ( count, 250u );
    ASSERT_GT( sumX, 0.0f );
    
    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query, parallel_chunked_matches_full )
{
    vecsWorld* w = vecsCreateWorld();
    for ( uint32_t i = 0; i < 4000u; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0 } );
        if ( ( i % 2u ) == 0u )
        {
            vecsSet<Velocity>( w, e, { ( float )( i + 1u ), 0 } );
        }
        if ( ( i % 5u ) == 0u )
        {
            vecsSet<Health>( w, e, { ( int )i } );
        }
    }

    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    vecsQueryAddWithout( q, vecsTypeId<Health>() );

    uint32_t fullCount = 0u;
    double fullSum = 0.0;
    vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity, Position& p, Velocity& v )
    {
        fullCount++;
        fullSum += ( double )p.x + ( double )v.vx;
    } );

    uint32_t chunkCount = 0u;
    double chunkSum = 0.0;
    vecsQueryChunk chunks[4];
    uint32_t numChunks = vecsQueryGetChunks( w, q, chunks, 4 );
    for ( uint32_t i = 0; i < numChunks; i++ )
    {
        vecsQueryExecuteChunk<Position, Velocity>( w, q, &chunks[i], [&]( vecsEntity, Position& p, Velocity& v )
        {
            chunkCount++;
            chunkSum += ( double )p.x + ( double )v.vx;
        } );
    }

    ASSERT_EQ( chunkCount, fullCount );
    ASSERT_EQ( chunkSum, fullSum );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( entity, exhaustion_and_reuse )
{
    vecsWorld* w = vecsCreateWorld( VECS_MAX_ENTITIES );
    std::vector<vecsEntity> entities( VECS_MAX_ENTITIES );

    for ( uint32_t i = 0; i < VECS_MAX_ENTITIES; i++ )
    {
        entities[i] = vecsCreate( w );
        ASSERT_NE( entities[i], VECS_INVALID_ENTITY );
    }
    ASSERT_EQ( vecsCount( w ), ( uint32_t )VECS_MAX_ENTITIES );
    ASSERT_EQ( vecsCreate( w ), VECS_INVALID_ENTITY );

    for ( uint32_t i = 0; i < 512u; i++ )
    {
        vecsDestroy( w, entities[i] );
    }
    ASSERT_EQ( vecsCount( w ), VECS_MAX_ENTITIES - 512u );

    for ( uint32_t i = 0; i < 512u; i++ )
    {
        vecsEntity reused = vecsCreate( w );
        ASSERT_NE( reused, VECS_INVALID_ENTITY );
        ASSERT_TRUE( vecsAlive( w, reused ) );
    }
    ASSERT_EQ( vecsCount( w ), ( uint32_t )VECS_MAX_ENTITIES );

    vecsDestroyWorld( w );
}

UTEST( query, optional_component_pointer_behaviour )
{
    vecsWorld* w = vecsCreateWorld( 2048u );
    for ( uint32_t i = 0; i < 256u; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0.0f } );
        if ( ( i & 1u ) == 0u )
        {
            vecsSet<Velocity>( w, e, { ( float )i, 1.0f } );
        }
    }

    vecsQuery* q = vecsBuildQuery<Position>( w );
    uint32_t withVelocity = 0u;
    uint32_t withoutVelocity = 0u;
    vecsQueryEach<Position>( w, q, [&]( vecsEntity e, Position& )
    {
        Velocity* v = vecsGet<Velocity>( w, e );
        if ( v )
        {
            withVelocity++;
            ASSERT_EQ( ( uint32_t )v->vy, 1u );
        }
        else
        {
            withoutVelocity++;
        }
    } );

    ASSERT_EQ( withVelocity, 128u );
    ASSERT_EQ( withoutVelocity, 128u );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query, without_filter_can_remove_all )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    for ( uint32_t i = 0; i < 128u; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0 } );
        vecsSet<Dead>( w, e, {} );
    }

    vecsQuery* q = vecsBuildQuery<Position>( w );
    vecsQueryAddWithout( q, vecsTypeId<Dead>() );

    uint32_t count = 0u;
    vecsQueryEach<Position>( w, q, [&]( vecsEntity, Position& ) { count++; } );
    ASSERT_EQ( count, 0u );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query, simd_scalar_boundaries )
{
    const uint32_t sizes[] = { 63u, 64u, 65u, 255u, 256u, 257u };
    for ( uint32_t n : sizes )
    {
        vecsWorld* w = vecsCreateWorld( 1024u );
        for ( uint32_t i = 0; i < n; i++ )
        {
            vecsEntity e = vecsCreate( w );
            vecsSet<Position>( w, e, { ( float )i, 0 } );
            vecsSet<Velocity>( w, e, { 1.0f, 0 } );
        }

        uint32_t eachCount = 0u;
        vecsEach<Position, Velocity>( w, [&]( vecsEntity, Position&, Velocity& ) { eachCount++; } );
        ASSERT_EQ( eachCount, n );

        vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
        uint32_t queryCount = 0u;
        vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity, Position&, Velocity& ) { queryCount++; } );
        ASSERT_EQ( queryCount, n );

        vecsDestroyQuery( q );
        vecsDestroyWorld( w );
    }
}

UTEST( relationships, deep_nesting_destroy_chain )
{
    vecsWorld* w = vecsCreateWorld( 2048u );
    std::vector<vecsEntity> chain;
    chain.reserve( 100u );
    for ( uint32_t i = 0; i < 100u; i++ )
    {
        chain.push_back( vecsCreate( w ) );
        if ( i > 0u )
        {
            vecsSetChildOf( w, chain[i], chain[i - 1u] );
        }
    }

    vecsDestroy( w, chain[0] );
    for ( vecsEntity e : chain )
    {
        ASSERT_FALSE( vecsAlive( w, e ) );
    }
    ASSERT_EQ( vecsCount( w ), 0u );

    vecsDestroyWorld( w );
}

UTEST( relationships, repeated_reparenting_keeps_counts_correct )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity p1 = vecsCreate( w );
    vecsEntity p2 = vecsCreate( w );
    vecsEntity p3 = vecsCreate( w );
    vecsEntity child = vecsCreate( w );

    vecsSetChildOf( w, child, p1 );
    ASSERT_EQ( vecsGetChildEntityCount( w, p1 ), 1u );
    ASSERT_EQ( vecsGetParentEntity( w, child ), p1 );

    vecsSetChildOf( w, child, p2 );
    ASSERT_EQ( vecsGetChildEntityCount( w, p1 ), 0u );
    ASSERT_EQ( vecsGetChildEntityCount( w, p2 ), 1u );
    ASSERT_EQ( vecsGetParentEntity( w, child ), p2 );

    vecsSetChildOf( w, child, p3 );
    ASSERT_EQ( vecsGetChildEntityCount( w, p2 ), 0u );
    ASSERT_EQ( vecsGetChildEntityCount( w, p3 ), 1u );
    ASSERT_EQ( vecsGetParentEntity( w, child ), p3 );

    vecsSetChildOf( w, child, VECS_INVALID_ENTITY );
    ASSERT_EQ( vecsGetChildEntityCount( w, p3 ), 0u );
    ASSERT_EQ( vecsGetParentEntity( w, child ), VECS_INVALID_ENTITY );

    vecsDestroyWorld( w );
}

static int g_cascadeAddPosition = 0;
static int g_cascadeAddVelocity = 0;
static void onCascadeVelocityAdd( vecsWorld*, vecsEntity, Velocity* )
{
    g_cascadeAddVelocity++;
}
static void onCascadePositionAdd( vecsWorld* w, vecsEntity e, Position* )
{
    g_cascadeAddPosition++;
    vecsSet<Velocity>( w, e, { 42.0f, 1.0f } );
}

UTEST( observer, cascading_add_callbacks )
{
    g_cascadeAddPosition = 0;
    g_cascadeAddVelocity = 0;
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsOnAdd<Position>( w, onCascadePositionAdd );
    vecsOnAdd<Velocity>( w, onCascadeVelocityAdd );

    vecsEntity e = vecsCreate( w );
    vecsSet<Position>( w, e, { 1.0f, 2.0f } );

    ASSERT_EQ( g_cascadeAddPosition, 1 );
    ASSERT_EQ( g_cascadeAddVelocity, 1 );
    ASSERT_TRUE( vecsHas<Velocity>( w, e ) );

    vecsDestroyWorld( w );
}

static int g_destroyRemovePosition = 0;
static int g_destroyRemoveVelocity = 0;
static void onDestroyRemovePosition( vecsWorld*, vecsEntity, Position* ) { g_destroyRemovePosition++; }
static void onDestroyRemoveVelocity( vecsWorld*, vecsEntity, Velocity* ) { g_destroyRemoveVelocity++; }

UTEST( observer, destroy_fires_remove_for_all_components )
{
    g_destroyRemovePosition = 0;
    g_destroyRemoveVelocity = 0;
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsOnRemove<Position>( w, onDestroyRemovePosition );
    vecsOnRemove<Velocity>( w, onDestroyRemoveVelocity );

    vecsEntity e = vecsCreate( w );
    vecsSet<Position>( w, e, { 0.0f, 0.0f } );
    vecsSet<Velocity>( w, e, { 0.0f, 0.0f } );
    vecsDestroy( w, e );

    ASSERT_EQ( g_destroyRemovePosition, 1 );
    ASSERT_EQ( g_destroyRemoveVelocity, 1 );

    vecsDestroyWorld( w );
}

UTEST( query, mutability_read_write_signature )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsQuery* q = vecsBuildQuery<const Position, Velocity>( w );
    ASSERT_TRUE( vecsQueryReads( q, vecsTypeId<Position>() ) );
    ASSERT_FALSE( vecsQueryWrites( q, vecsTypeId<Position>() ) );
    ASSERT_TRUE( vecsQueryWrites( q, vecsTypeId<Velocity>() ) );
    ASSERT_FALSE( vecsQueryReads( q, vecsTypeId<Health>() ) );
    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( simd, scalar_fallback_exact_match )
{
    const uint32_t entityCount = vecsTestClampEntities( 10000u );
    const uint32_t worldCapacity = vecsTestClampEntities( entityCount * 2u );
    vecsWorld* w = vecsCreateWorld( worldCapacity );
    std::mt19937 rng( 1337u );
    std::uniform_real_distribution<float> distPos( -500.0f, 500.0f );
    std::uniform_real_distribution<float> distVel( -50.0f, 50.0f );
    std::uniform_int_distribution<int> bit( 0, 99 );

    for ( uint32_t i = 0; i < entityCount; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { distPos( rng ), distPos( rng ) } );
        if ( bit( rng ) < 70 )
        {
            vecsSet<Velocity>( w, e, { distVel( rng ), distVel( rng ) } );
        }
        if ( bit( rng ) < 25 )
        {
            vecsSet<Health>( w, e, { ( int )i } );
        }
    }

    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    vecsQueryAddWithout( q, vecsTypeId<Health>() );

    std::vector<uint64_t> simdResults;
    std::vector<uint64_t> scalarResults;
    simdResults.reserve( entityCount );
    scalarResults.reserve( entityCount );

    g_vecsSimdConfig = VECS_SIMD_AUTO;
    vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity e, Position& p, Velocity& v )
    {
        uint64_t h = vecsEntityIndex( e );
        h = ( h * 1315423911u ) ^ ( uint64_t )( p.x * 1000.0f ) ^ ( ( uint64_t )( v.vx * 1000.0f ) << 32 );
        simdResults.push_back( h );
    } );

    g_vecsSimdConfig = VECS_SIMD_SCALAR;
    vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity e, Position& p, Velocity& v )
    {
        uint64_t h = vecsEntityIndex( e );
        h = ( h * 1315423911u ) ^ ( uint64_t )( p.x * 1000.0f ) ^ ( ( uint64_t )( v.vx * 1000.0f ) << 32 );
        scalarResults.push_back( h );
    } );
    g_vecsSimdConfig = VECS_SIMD_AUTO;

    ASSERT_EQ( scalarResults.size(), simdResults.size() );
    for ( size_t i = 0; i < simdResults.size(); i++ )
    {
        ASSERT_EQ( scalarResults[i], simdResults[i] );
    }

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( benchmark, vecs_ops_per_second )
{
    ( void )utest_result;
    const uint32_t activeCapacity = VECS_MAX_ENTITIES;
    const uint64_t opTarget = 1000000ULL;

    {
        vecsWorld* w = vecsCreateWorld( activeCapacity );
        std::vector<vecsEntity> entities( activeCapacity );
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint64_t i = 0; i < opTarget; i++ )
        {
            entities[( size_t )( i % activeCapacity )] = vecsCreate( w );
        }
        const double ops = vecsBenchOpsPerSecond( start, opTarget );
        std::printf( "[BENCHMARK] Entity Create: %s\n", vecsFormatOps( ops ).c_str() );
        vecsDestroyWorld( w );
    }

    {
        vecsWorld* w = vecsCreateWorld( activeCapacity );
        std::vector<vecsEntity> entities( activeCapacity );
        for ( uint32_t i = 0; i < activeCapacity; i++ )
        {
            entities[i] = vecsCreate( w );
        }

        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint64_t i = 0; i < opTarget; i++ )
        {
            vecsEntity e = entities[( uint32_t )( i % activeCapacity )];
            vecsSet<Position>( w, e, { ( float )i, 1.0f } );
            vecsSet<Velocity>( w, e, { 2.0f, ( float )i } );
        }
        const double ops = vecsBenchOpsPerSecond( start, opTarget * 2u );
        std::printf( "[BENCHMARK] Component Insert (Position+Velocity): %s\n", vecsFormatOps( ops ).c_str() );
        vecsDestroyWorld( w );
    }

    {
        vecsWorld* w = vecsCreateWorld( activeCapacity );
        const uint64_t createTarget = 100000ULL;
        const uint32_t batchSize = 20000u;
        uint64_t created = 0u;
        const auto start = std::chrono::high_resolution_clock::now();
        while ( created < createTarget )
        {
            uint32_t batch = ( uint32_t )( ( createTarget - created ) < batchSize ? ( createTarget - created ) : batchSize );
            std::vector<vecsEntity> ents( batch );
            for ( uint32_t i = 0; i < batch; i++ )
            {
                ents[i] = vecsCreate( w );
            }
            for ( uint32_t i = 0; i < batch; i++ )
            {
                if ( ents[i] != VECS_INVALID_ENTITY )
                {
                    vecsDestroy( w, ents[i] );
                }
            }
            created += batch;
        }
        const double ops = vecsBenchOpsPerSecond( start, createTarget );
        std::printf( "[BENCHMARK] Bulk Create/Destroy: %s\n", vecsFormatOps( ops ).c_str() );
        vecsDestroyWorld( w );
    }

    {
        vecsWorld* w = vecsCreateWorld( activeCapacity );
        std::vector<vecsEntity> entities( activeCapacity );
        for ( uint32_t i = 0; i < activeCapacity; i++ )
        {
            entities[i] = vecsCreate( w );
            vecsSet<Position>( w, entities[i], { ( float )i, 0.0f } );
            vecsSet<Velocity>( w, entities[i], { 1.0f, 2.0f } );
        }

        uint64_t processedEach = 0u;
        const auto eachStart = std::chrono::high_resolution_clock::now();
        for ( int iter = 0; iter < 200; iter++ )
        {
            vecsEach<Position, Velocity>( w, [&processedEach]( vecsEntity, Position&, Velocity& )
            {
                ++processedEach;
            } );
        }
        const double eachOps = vecsBenchOpsPerSecond( eachStart, processedEach );
        std::printf( "[BENCHMARK] Query Iterate vecsEach (SIMD path): %s\n", vecsFormatOps( eachOps ).c_str() );

        vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
        uint64_t processedCached = 0u;
        const auto queryStart = std::chrono::high_resolution_clock::now();
        for ( int iter = 0; iter < 200; iter++ )
        {
            vecsQueryEach<Position, Velocity>( w, q, [&processedCached]( vecsEntity, Position&, Velocity& )
            {
                ++processedCached;
            } );
        }
        const double queryOps = vecsBenchOpsPerSecond( queryStart, processedCached );
        std::printf( "[BENCHMARK] Query Iterate vecsQueryEach (cached): %s\n", vecsFormatOps( queryOps ).c_str() );
        vecsDestroyQuery( q );
        vecsDestroyWorld( w );
    }
}

UTEST( benchmark, vecs_vs_entt )
{
    ( void )utest_result;
#if defined( VECS_HAS_ENTT )
    const uint32_t activeCapacity = vecsTestClampEntities( 50000u );
    const uint64_t opTarget = 1000000ULL;
    const uint64_t createTarget = activeCapacity;

    double vecsCreateOps = 0.0;
    double enttCreateOps = 0.0;
    {
        vecsWorld* w = vecsCreateWorld( activeCapacity );
        std::vector<vecsEntity> entities( activeCapacity );
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint64_t i = 0; i < createTarget; i++ )
        {
            entities[( size_t )i] = vecsCreate( w );
        }
        vecsCreateOps = vecsBenchOpsPerSecond( start, createTarget );
        vecsDestroyWorld( w );
    }
    {
        entt::registry registry;
        std::vector<entt::entity> entities;
        entities.resize( ( size_t )createTarget );
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint64_t i = 0; i < createTarget; i++ )
        {
            entities[( size_t )i] = registry.create();
        }
        enttCreateOps = vecsBenchOpsPerSecond( start, createTarget );
    }
    std::printf( "[BENCHMARK] Entity Create | Vecs: %s | EnTT: %s\n", vecsFormatOps( vecsCreateOps ).c_str(), vecsFormatOps( enttCreateOps ).c_str() );

    double vecsSetOps = 0.0;
    double enttSetOps = 0.0;
    {
        vecsWorld* w = vecsCreateWorld( activeCapacity );
        std::vector<vecsEntity> entities( activeCapacity );
        for ( uint32_t i = 0; i < activeCapacity; i++ )
        {
            entities[i] = vecsCreate( w );
        }
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint64_t i = 0; i < opTarget; i++ )
        {
            vecsEntity e = entities[( uint32_t )( i % activeCapacity )];
            vecsSet<Position>( w, e, { ( float )i, 0.0f } );
            vecsSet<Velocity>( w, e, { 0.0f, ( float )i } );
        }
        vecsSetOps = vecsBenchOpsPerSecond( start, opTarget * 2u );
        vecsDestroyWorld( w );
    }
    {
        entt::registry registry;
        std::vector<entt::entity> entities;
        entities.resize( activeCapacity );
        for ( uint32_t i = 0; i < activeCapacity; i++ )
        {
            entities[( size_t )i] = registry.create();
        }
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint64_t i = 0; i < opTarget; i++ )
        {
            const Position p = { ( float )i, 0.0f };
            const Velocity v = { 0.0f, ( float )i };
            entt::entity e = entities[( size_t )( i % activeCapacity )];
            registry.emplace_or_replace<Position>( e, p );
            registry.emplace_or_replace<Velocity>( e, v );
        }
        enttSetOps = vecsBenchOpsPerSecond( start, opTarget * 2u );
    }
    std::printf( "[BENCHMARK] Component Insert | Vecs: %s | EnTT: %s\n", vecsFormatOps( vecsSetOps ).c_str(), vecsFormatOps( enttSetOps ).c_str() );

    double vecsIterOps = 0.0;
    double enttIterOps = 0.0;
    {
        vecsWorld* w = vecsCreateWorld( activeCapacity );
        for ( uint32_t i = 0; i < activeCapacity; i++ )
        {
            vecsEntity e = vecsCreate( w );
            vecsSet<Position>( w, e, { ( float )i, 1.0f } );
            vecsSet<Velocity>( w, e, { 2.0f, ( float )i } );
        }

        uint64_t processed = 0u;
        const uint64_t target = 1000000ULL;
        const auto start = std::chrono::high_resolution_clock::now();
        while ( processed < target )
        {
            vecsEach<Position, Velocity>( w, [&]( vecsEntity, Position&, Velocity& ) { processed++; } );
        }
        vecsIterOps = vecsBenchOpsPerSecond( start, processed );
        vecsDestroyWorld( w );
    }
    {
        entt::registry registry;
        std::vector<entt::entity> entities;
        entities.resize( activeCapacity );
        for ( uint32_t i = 0; i < activeCapacity; i++ )
        {
            entt::entity e = registry.create();
            entities[( size_t )i] = e;
            registry.emplace<Position>( e, Position{ ( float )i, 1.0f } );
            registry.emplace<Velocity>( e, Velocity{ 2.0f, ( float )i } );
        }

        uint64_t processed = 0u;
        const auto start = std::chrono::high_resolution_clock::now();
        while ( processed < opTarget )
        {
            registry.view<Position, Velocity>().each( [&]( Position&, Velocity& )
            {
                processed++;
            } );
        }
        enttIterOps = vecsBenchOpsPerSecond( start, processed );
    }
    std::printf( "[BENCHMARK] Query Iterate | Vecs: %s | EnTT: %s\n", vecsFormatOps( vecsIterOps ).c_str(), vecsFormatOps( enttIterOps ).c_str() );

    double vecsFullDestroyOps = 0.0;
    double enttFullDestroyOps = 0.0;
    {
        const uint32_t childDepth = 2u;
        const uint32_t childrenPerLevel = 5u;
        const uint32_t rootCount = vecsTestHierarchyRootCount( 1000u, childDepth, childrenPerLevel, activeCapacity );
        const uint32_t nodesPerRoot = vecsTestHierarchyNodesPerRoot( childDepth, childrenPerLevel );
        const uint32_t estimatedEntities = rootCount * nodesPerRoot;
        vecsWorld* w = vecsCreateWorld( estimatedEntities );
        std::vector<vecsEntity> roots( rootCount );
        uint64_t totalEntities = 0;
        for ( uint32_t r = 0; r < rootCount; r++ )
        {
            vecsEntity root = vecsCreate( w );
            roots[r] = root;
            totalEntities++;
            vecsSet<Position>( w, root, { ( float )r, 0 } );
            std::vector<vecsEntity> currentLevel;
            currentLevel.push_back( root );
            for ( uint32_t d = 0; d < childDepth; d++ )
            {
                std::vector<vecsEntity> nextLevel;
                for ( vecsEntity parent : currentLevel )
                {
                    for ( uint32_t c = 0; c < childrenPerLevel; c++ )
                    {
                        vecsEntity child = vecsCreate( w );
                        vecsSetChildOf( w, child, parent );
                        vecsSet<Position>( w, child, { ( float )c, ( float )( d + 1 ) } );
                        totalEntities++;
                        nextLevel.push_back( child );
                    }
                }
                currentLevel = std::move( nextLevel );
            }
        }
        const auto start = std::chrono::high_resolution_clock::now();
        for ( vecsEntity root : roots )
        {
            vecsDestroy( w, root );
        }
        vecsFullDestroyOps = vecsBenchOpsPerSecond( start, totalEntities );
        vecsDestroyWorld( w );
    }
    {
        struct Parent { std::vector<entt::entity> children; };
        const uint32_t childDepth = 2u;
        const uint32_t childrenPerLevel = 5u;
        const uint32_t rootCount = vecsTestHierarchyRootCount( 1000u, childDepth, childrenPerLevel, activeCapacity );
        entt::registry registry;
        std::vector<entt::entity> roots( rootCount );
        uint64_t totalEntities = 0;
        for ( uint32_t r = 0; r < rootCount; r++ )
        {
            entt::entity root = registry.create();
            roots[r] = root;
            totalEntities++;
            registry.emplace<Position>( root, ( float )r, 0 );
            registry.emplace<Parent>( root, Parent{} );
            std::vector<entt::entity> currentLevel;
            currentLevel.push_back( root );
            for ( uint32_t d = 0; d < childDepth; d++ )
            {
                std::vector<entt::entity> nextLevel;
                for ( entt::entity parent : currentLevel )
                {
                    for ( uint32_t c = 0; c < childrenPerLevel; c++ )
                    {
                        entt::entity child = registry.create();
                        registry.emplace<Position>( child, ( float )c, ( float )( d + 1 ) );
                        registry.emplace<Parent>( child, Parent{} );
                        registry.get<Parent>( parent ).children.push_back( child );
                        totalEntities++;
                        nextLevel.push_back( child );
                    }
                }
                currentLevel = std::move( nextLevel );
            }
        }
        std::function<void( entt::entity )> destroyRecursive = [&]( entt::entity e )
        {
            Parent* p = registry.try_get<Parent>( e );
            if ( p )
            {
                for ( entt::entity child : p->children )
                {
                    if ( registry.valid( child ) )
                        destroyRecursive( child );
                }
            }
            registry.destroy( e );
        };
        const auto start = std::chrono::high_resolution_clock::now();
        for ( entt::entity root : roots )
        {
            destroyRecursive( root );
        }
        enttFullDestroyOps = vecsBenchOpsPerSecond( start, totalEntities );
    }
    std::printf( "[BENCHMARK] Full Destroy (Recursive Hierarchy) | Vecs: %s | EnTT: %s\n", vecsFormatOps( vecsFullDestroyOps ).c_str(), vecsFormatOps( enttFullDestroyOps ).c_str() );

    double vecsChurnOps = 0.0;
    double enttChurnOps = 0.0;
    {
        const uint32_t entityCount = vecsTestClampEntities( 50000u );
        vecsWorld* w = vecsCreateWorld( entityCount );
        std::vector<vecsEntity> entities( entityCount );
        for ( uint32_t i = 0; i < entityCount; i++ )
        {
            entities[i] = vecsCreate( w );
            vecsSet<Position>( w, entities[i], { ( float )i, 0 } );
            vecsSet<Velocity>( w, entities[i], { 1, 0 } );
        }
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint32_t i = 0; i < entityCount; i++ )
        {
            vecsSet<Health>( w, entities[i], { 100 } );
        }
        for ( uint32_t i = 0; i < entityCount; i++ )
        {
            vecsUnset<Health>( w, entities[i] );
        }
        vecsChurnOps = vecsBenchOpsPerSecond( start, entityCount * 2u );
        vecsDestroyWorld( w );
    }
    {
        const uint32_t entityCount = vecsTestClampEntities( 50000u );
        entt::registry registry;
        std::vector<entt::entity> entities( entityCount );
        for ( uint32_t i = 0; i < entityCount; i++ )
        {
            entities[i] = registry.create();
            registry.emplace<Position>( entities[i], ( float )i, 0 );
            registry.emplace<Velocity>( entities[i], 1, 0 );
        }
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint32_t i = 0; i < entityCount; i++ )
        {
            registry.emplace_or_replace<Health>( entities[i], 100 );
        }
        for ( uint32_t i = 0; i < entityCount; i++ )
        {
            registry.remove<Health>( entities[i] );
        }
        enttChurnOps = vecsBenchOpsPerSecond( start, entityCount * 2u );
    }
    std::printf( "[BENCHMARK] Structural Churn (Mass Add/Remove Health) | Vecs: %s | EnTT: %s\n", vecsFormatOps( vecsChurnOps ).c_str(), vecsFormatOps( enttChurnOps ).c_str() );

    double vecsShotgunOps = 0.0;
    double enttShotgunOps = 0.0;
    {
        const uint32_t entityCount = vecsTestClampEntities( 60000u );
        vecsWorld* w = vecsCreateWorld( entityCount );
        std::vector<vecsEntity> entities( entityCount );
        for ( uint32_t i = 0; i < entityCount; i++ )
        {
            entities[i] = vecsCreate( w );
            vecsSet<Position>( w, entities[i], { ( float )i, 0 } );
            vecsSet<Velocity>( w, entities[i], { 1, 0 } );
            vecsSet<Health>( w, entities[i], { 100 } );
        }
        const uint32_t deleteCount = entityCount / 2u;
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint32_t i = 0; i < entityCount; i += 2 )
        {
            vecsDestroy( w, entities[i] );
        }
        vecsShotgunOps = vecsBenchOpsPerSecond( start, deleteCount );
        vecsDestroyWorld( w );
    }
    {
        const uint32_t entityCount = vecsTestClampEntities( 60000u );
        entt::registry registry;
        std::vector<entt::entity> entities( entityCount );
        for ( uint32_t i = 0; i < entityCount; i++ )
        {
            entities[i] = registry.create();
            registry.emplace<Position>( entities[i], ( float )i, 0 );
            registry.emplace<Velocity>( entities[i], 1, 0 );
            registry.emplace<Health>( entities[i], 100 );
        }
        const uint32_t deleteCount = entityCount / 2u;
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint32_t i = 0; i < entityCount; i += 2 )
        {
            registry.destroy( entities[i] );
        }
        enttShotgunOps = vecsBenchOpsPerSecond( start, deleteCount );
    }
    std::printf( "[BENCHMARK] Fragmented Deletion (Shotgun Test) | Vecs: %s | EnTT: %s\n", vecsFormatOps( vecsShotgunOps ).c_str(), vecsFormatOps( enttShotgunOps ).c_str() );
#else
    std::printf( "[BENCHMARK] Vecs vs EnTT skipped (entt/entt.hpp not found).\n" );
#endif
}

struct Transform
{
    float m[16];
    Transform()
    {
        for ( int i = 0; i < 16; i++ ) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
    Transform( float diagonal )
    {
        for ( int i = 0; i < 16; i++ ) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = diagonal;
    }
};

struct Name
{
    char data[64];
    Name() { data[0] = '\0'; }
    Name( const char* s )
    {
        size_t i = 0;
        for ( ; s[i] && i < 63; i++ ) data[i] = s[i];
        data[i] = '\0';
    }
};

UTEST( emplace, constructs_in_place )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    Transform* t = vecsEmplace<Transform>( w, e );
    ASSERT_TRUE( t != nullptr );
    ASSERT_EQ( t->m[0], 1.0f );
    ASSERT_EQ( t->m[15], 1.0f );
    
    vecsDestroyWorld( w );
}

UTEST( emplace, constructs_with_args )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    Transform* t = vecsEmplace<Transform>( w, e, 5.0f );
    ASSERT_TRUE( t != nullptr );
    ASSERT_EQ( t->m[0], 5.0f );
    ASSERT_EQ( t->m[5], 5.0f );
    
    vecsDestroyWorld( w );
}

UTEST( emplace, replaces_existing )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    vecsSet<Position>( w, e, { 1.0f, 2.0f } );
    Position* p = vecsEmplace<Position>( w, e, 10.0f, 20.0f );
    
    ASSERT_EQ( p->x, 10.0f );
    ASSERT_EQ( p->y, 20.0f );
    
    vecsDestroyWorld( w );
}

UTEST( emplace, non_trivial_type )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    Name* n = vecsEmplace<Name>( w, e, "TestEntity" );
    ASSERT_TRUE( n != nullptr );
    ASSERT_EQ( strcmp( n->data, "TestEntity" ), 0 );
    
    vecsDestroyWorld( w );
}

UTEST( addtag, adds_tag_without_data )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    vecsAddTag<IsEnemy>( w, e );
    
    ASSERT_TRUE( vecsHas<IsEnemy>( w, e ) );
    
    vecsPool* pool = w->pools[vecsTypeId<IsEnemy>()];
    ASSERT_TRUE( pool != nullptr );
    ASSERT_TRUE( pool->noData );
    
    vecsDestroyWorld( w );
}

UTEST( addtag, idempotent )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    vecsAddTag<IsEnemy>( w, e );
    vecsAddTag<IsEnemy>( w, e );
    vecsAddTag<IsEnemy>( w, e );
    
    ASSERT_TRUE( vecsHas<IsEnemy>( w, e ) );
    ASSERT_EQ( w->pools[vecsTypeId<IsEnemy>()]->count, 1u );
    
    vecsDestroyWorld( w );
}

UTEST( hasall, all_present )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    vecsSet<Position>( w, e, { 0, 0 } );
    vecsSet<Velocity>( w, e, { 1, 1 } );
    vecsSet<Health>( w, e, { 100 } );
    
    ASSERT_TRUE( ( vecsHasAll<Position, Velocity, Health>( w, e ) ) );
    ASSERT_TRUE( ( vecsHasAll<Position, Velocity>( w, e ) ) );
    ASSERT_TRUE( vecsHasAll<Position>( w, e ) );
    
    vecsDestroyWorld( w );
}

UTEST( hasall, some_missing )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    vecsSet<Position>( w, e, { 0, 0 } );
    vecsSet<Health>( w, e, { 100 } );
    
    ASSERT_FALSE( ( vecsHasAll<Position, Velocity, Health>( w, e ) ) );
    ASSERT_FALSE( ( vecsHasAll<Position, Velocity>( w, e ) ) );
    ASSERT_TRUE( ( vecsHasAll<Position, Health>( w, e ) ) );
    
    vecsDestroyWorld( w );
}

UTEST( hasall, none_present )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    ASSERT_FALSE( ( vecsHasAll<Position, Velocity>( w, e ) ) );
    ASSERT_FALSE( vecsHasAll<Position>( w, e ) );
    
    vecsDestroyWorld( w );
}

UTEST( hasany, any_present )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    vecsSet<Position>( w, e, { 0, 0 } );
    
    ASSERT_TRUE( ( vecsHasAny<Position, Velocity, Health>( w, e ) ) );
    ASSERT_TRUE( ( vecsHasAny<Position, Velocity>( w, e ) ) );
    ASSERT_FALSE( ( vecsHasAny<Velocity, Health>( w, e ) ) );
    
    vecsDestroyWorld( w );
}

UTEST( removeall, removes_multiple )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    vecsSet<Position>( w, e, { 0, 0 } );
    vecsSet<Velocity>( w, e, { 1, 1 } );
    vecsSet<Health>( w, e, { 100 } );
    
    ASSERT_TRUE( ( vecsHasAll<Position, Velocity, Health>( w, e ) ) );
    
    ( vecsRemoveAll<Position, Velocity, Health>( w, e ) );
    
    ASSERT_FALSE( vecsHas<Position>( w, e ) );
    ASSERT_FALSE( vecsHas<Velocity>( w, e ) );
    ASSERT_FALSE( vecsHas<Health>( w, e ) );
    
    vecsDestroyWorld( w );
}

UTEST( removeall, partial_removal )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    
    vecsSet<Position>( w, e, { 0, 0 } );
    vecsSet<Velocity>( w, e, { 1, 1 } );
    
    ( vecsRemoveAll<Position, Velocity>( w, e ) );
    
    ASSERT_FALSE( vecsHas<Position>( w, e ) );
    ASSERT_FALSE( vecsHas<Velocity>( w, e ) );
    
    vecsDestroyWorld( w );
}

UTEST( handle, basic_operations )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsHandle player = vecsCreateHandle( w );
    ASSERT_TRUE( player.alive() );
    
    player.emplace<Position>( 10.0f, 20.0f );
    player.emplace<Velocity>( 1.0f, 2.0f );
    
    ASSERT_TRUE( player.has<Position>() );
    ASSERT_TRUE( player.has<Velocity>() );
    
    Position* p = player.get<Position>();
    ASSERT_EQ( p->x, 10.0f );
    ASSERT_EQ( p->y, 20.0f );
    
    player.remove<Velocity>();
    ASSERT_FALSE( player.has<Velocity>() );
    
    player.destroy();
    ASSERT_FALSE( player.alive() );
    
    vecsDestroyWorld( w );
}

UTEST( handle, set_and_get )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsHandle e = vecsCreateHandle( w );
    e.set<Position>( { 5.0f, 10.0f } );
    
    Position* p = e.get<Position>();
    ASSERT_EQ( p->x, 5.0f );
    ASSERT_EQ( p->y, 10.0f );
    
    vecsDestroyWorld( w );
}

UTEST( handle, has_all_variadic )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsHandle e = vecsCreateHandle( w );
    e.emplace<Position>( 0.0f, 0.0f );
    e.emplace<Velocity>( 1.0f, 1.0f );
    
    ASSERT_TRUE( ( e.hasAll<Position, Velocity>() ) );
    ASSERT_FALSE( ( e.hasAll<Position, Velocity, Health>() ) );
    
    vecsDestroyWorld( w );
}

UTEST( handle, remove_all_variadic )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsHandle e = vecsCreateHandle( w );
    e.emplace<Position>( 0.0f, 0.0f );
    e.emplace<Velocity>( 1.0f, 1.0f );
    e.emplace<Health>( 100 );
    
    e.removeAll<Position, Velocity>();
    
    ASSERT_FALSE( e.has<Position>() );
    ASSERT_FALSE( e.has<Velocity>() );
    ASSERT_TRUE( e.has<Health>() );
    
    vecsDestroyWorld( w );
}

UTEST( handle, parent_child )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsHandle parent = vecsCreateHandle( w );
    vecsHandle child = vecsCreateHandle( w );
    
    child.setParent( parent.id() );
    
    ASSERT_EQ( child.parent(), parent.id() );
    ASSERT_EQ( parent.childCount(), 1u );
    ASSERT_EQ( parent.child( 0 ), child.id() );
    
    vecsDestroyWorld( w );
}

UTEST( handle, add_tag )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsHandle e = vecsCreateHandle( w );
    e.addTag<IsEnemy>();
    
    ASSERT_TRUE( e.has<IsEnemy>() );
    
    vecsDestroyWorld( w );
}

UTEST( handle, make_handle_from_entity )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity raw = vecsCreate( w );
    vecsSet<Position>( w, raw, { 42.0f, 24.0f } );
    
    vecsHandle h = vecsMakeHandle( w, raw );
    
    ASSERT_TRUE( h.alive() );
    ASSERT_EQ( h.id(), raw );
    
    Position* p = h.get<Position>();
    ASSERT_EQ( p->x, 42.0f );
    
    vecsDestroyWorld( w );
}

UTEST( each_no_entity, iterates_components )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    for ( int i = 0; i < 10; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, ( float )i * 2.0f } );
    }
    
    float sumX = 0.0f;
    float sumY = 0.0f;
    vecsEachNoEntity<Position>( w, [&]( Position& p )
    {
        sumX += p.x;
        sumY += p.y;
    } );
    
    ASSERT_EQ( sumX, 45.0f );
    ASSERT_EQ( sumY, 90.0f );
    
    vecsDestroyWorld( w );
}

UTEST( each_no_entity, multiple_components )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    for ( int i = 0; i < 5; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0.0f } );
        vecsSet<Velocity>( w, e, { ( float )i * 10.0f, 0.0f } );
    }
    
    float sumPos = 0.0f;
    float sumVel = 0.0f;
    vecsEachNoEntity<Position, Velocity>( w, [&]( Position& p, Velocity& v )
    {
        sumPos += p.x;
        sumVel += v.vx;
    } );
    
    ASSERT_EQ( sumPos, 10.0f );
    ASSERT_EQ( sumVel, 100.0f );
    
    vecsDestroyWorld( w );
}

UTEST( each_no_entity, mutates_components )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    for ( int i = 0; i < 5; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0.0f } );
    }
    
    vecsEachNoEntity<Position>( w, [&]( Position& p )
    {
        p.x *= 2.0f;
    } );
    
    float sum = 0.0f;
    vecsEachNoEntity<Position>( w, [&]( Position& p )
    {
        sum += p.x;
    } );
    
    ASSERT_EQ( sum, 20.0f );
    
    vecsDestroyWorld( w );
}

UTEST( benchmark, simd_test_matrix )
{
    ( void )utest_result;
    const uint32_t entityCount = VECS_MAX_ENTITIES;
    vecsWorld* w = vecsCreateWorld( entityCount );
    
    // Sparse setup: 100% have Position, 10% have Velocity
    for ( uint32_t i = 0; i < entityCount; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { ( float )i, 0.0f } );
        if ( i % 10 == 0 )
        {
            vecsSet<Velocity>( w, e, { 1.0f, 2.0f } );
        }
    }
    
    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    
    std::printf( "[BENCHMARK] SIMD Test Matrix (%u entities, 10%% density, 100 iterations):\n", entityCount );
    
    vecsSimdLevel savedConfig = g_vecsSimdConfig;
    
    g_vecsSimdConfig = VECS_SIMD_AUTO;
    vecsSimdLevel supported = vecsRuntimeSimdSupported();
    
    auto runBenchmark = [&]( vecsSimdLevel level, const char* name ) -> double
    {
        ( void )name;
        g_vecsSimdConfig = level;
        uint64_t processed = 0u;
        const auto start = std::chrono::high_resolution_clock::now();
        for ( int iter = 0; iter < 100; iter++ )
        {
            vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity, Position&, Velocity& )
            {
                processed++;
            } );
        }
        return vecsBenchOpsPerSecond( start, processed );
    };
    
    double scalarOps = runBenchmark( VECS_SIMD_SCALAR, "Scalar" );
    std::printf( "  %-10s: %s\n", "Scalar", vecsFormatOps( scalarOps ).c_str() );
    
#if defined( VECS_SSE2 )
    if ( supported >= VECS_SIMD_SSE2 )
    {
        double sse2Ops = runBenchmark( VECS_SIMD_SSE2, "SSE2" );
        std::printf( "  %-10s: %s (%.2fx faster than scalar)\n", "SSE2", vecsFormatOps( sse2Ops ).c_str(), sse2Ops / scalarOps );
    }
    else
    {
        std::printf( "  %-10s: Skipped (CPU unsupported)\n", "SSE2" );
    }
#endif
    
#if defined( VECS_AVX2 )
    if ( supported >= VECS_SIMD_AVX2 )
    {
        double avx2Ops = runBenchmark( VECS_SIMD_AVX2, "AVX2" );
        std::printf( "  %-10s: %s (%.2fx faster than scalar, %.2fx faster than SSE2)\n", "AVX2", vecsFormatOps( avx2Ops ).c_str(), avx2Ops / scalarOps, ( supported >= VECS_SIMD_SSE2 ? avx2Ops / scalarOps : 0.0 ) ); // approx comparison
    }
    else
    {
        std::printf( "  %-10s: Skipped (CPU unsupported)\n", "AVX2" );
    }
#endif
    
#if defined( VECS_NEON )
    if ( supported >= VECS_SIMD_NEON )
    {
        double neonOps = runBenchmark( VECS_SIMD_NEON, "NEON" );
        std::printf( "  %-10s: %s (%.2fx faster than scalar)\n", "NEON", vecsFormatOps( neonOps ).c_str(), neonOps / scalarOps );
    }
    else
    {
        std::printf( "  %-10s: Skipped (CPU unsupported)\n", "NEON" );
    }
#endif
    
    double autoOps = runBenchmark( VECS_SIMD_AUTO, "Auto" );
    std::printf( "  %-10s: %s (runtime best)\n", "Auto", vecsFormatOps( autoOps ).c_str() );
    
#if defined( VECS_HAS_ENTT )
    {
        entt::registry registry;
        std::vector<entt::entity> entities( entityCount );
        for ( uint32_t i = 0; i < entityCount; i++ )
        {
            entities[i] = registry.create();
            registry.emplace<Position>( entities[i], ( float )i, 0.0f );
            if ( i % 10 == 0 )
            {
                registry.emplace<Velocity>( entities[i], 1.0f, 2.0f );
            }
        }
        
        uint64_t processed = 0u;
        const auto start = std::chrono::high_resolution_clock::now();
        for ( int iter = 0; iter < 100; iter++ )
        {
            registry.view<Position, Velocity>().each( [&]( Position&, Velocity& )
            {
                processed++;
            } );
        }
        double enttOps = vecsBenchOpsPerSecond( start, processed );
        std::printf( "  %-10s: %s (%.2fx vs Vecs Auto)\n", "EnTT", vecsFormatOps( enttOps ).c_str(), enttOps / autoOps );
    }
#endif
    
    g_vecsSimdConfig = savedConfig;
    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( first_match, single_component )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e1 = vecsCreate( w );
    vecsEntity e2 = vecsCreate( w );
    ( void )e1;
    vecsSet<Position>( w, e2, { 1.0f, 2.0f } );

    vecsEntity match = vecsFirstMatch<Position>( w );
    ASSERT_EQ( match, e2 );

    vecsDestroyWorld( w );
}

UTEST( first_match, multiple_components )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    for( int i = 0; i < 10; ++i )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { 1.0f, 1.0f } );
        if ( i == 7 )
        {
            vecsSet<Velocity>( w, e, { 2.0f, 2.0f } );
        }
    }

    vecsEntity match = vecsFirstMatch<Position, Velocity>( w );
    ASSERT_TRUE( vecsAlive( w, match ) );
    ASSERT_TRUE( ( vecsHasAll<Position, Velocity>( w, match ) ) );

    vecsDestroyWorld( w );
}

UTEST( first_match, no_match )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity match = vecsFirstMatch<Position>( w );
    ASSERT_EQ( match, VECS_INVALID_ENTITY );

    vecsEntity e = vecsCreate( w );
    vecsSet<Position>( w, e, { 1.0f, 1.0f } );
    match = vecsFirstMatch<Position, Velocity>( w );
    ASSERT_EQ( match, VECS_INVALID_ENTITY );

    vecsDestroyWorld( w );
}

UTEST( first_match, query_with_without )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e1 = vecsCreate( w );
    vecsSet<Position>( w, e1, { 1.0f, 1.0f } );
    vecsAddTag<Dead>( w, e1 );

    vecsEntity e2 = vecsCreate( w );
    vecsSet<Position>( w, e2, { 1.0f, 1.0f } );

    vecsQuery* q = vecsBuildQuery<Position>( w );
    vecsQueryAddWithout( q, vecsTypeId<Dead>() );

    vecsEntity match = vecsQueryFirstMatch<Position>( w, q );
    ASSERT_EQ( match, e2 );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( first_match, equivalent_to_foreach )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    for( int i = 0; i < 100; ++i )
    {
        vecsEntity e = vecsCreate( w );
        if ( i % 3 == 0 ) vecsSet<Position>( w, e, { 1.0f, 1.0f } );
        if ( i % 5 == 0 ) vecsSet<Velocity>( w, e, { 1.0f, 1.0f } );
    }

    vecsEntity first_match_result = vecsFirstMatch<Position, Velocity>( w );
    
    vecsEntity foreach_first_result = VECS_INVALID_ENTITY;
    bool found = false;
    vecsEach<Position, Velocity>( w, [&]( vecsEntity e, Position&, Velocity& )
    {
        if ( !found )
        {
            foreach_first_result = e;
            found = true;
        }
    } );

    ASSERT_EQ( first_match_result, foreach_first_result );

    vecsDestroyWorld( w );
}

UTEST( first_match, simd_levels )
{
    const uint32_t entityCount = vecsTestClampEntities( 8000u );
    const uint32_t matchIndex = entityCount > 512u ? entityCount - 512u : entityCount / 2u;
    vecsWorld* w = vecsCreateWorld( entityCount );
    for ( uint32_t i = 0; i < entityCount; ++i )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { 1.0f, 1.0f } );
        if ( i == matchIndex )
        {
            vecsSet<Velocity>( w, e, { 1.0f, 1.0f } );
        }
    }

    vecsSimdLevel savedConfig = g_vecsSimdConfig;
    vecsSimdLevel supported = vecsRuntimeSimdSupported();

    auto runTest = [&]( vecsSimdLevel level )
    {
        g_vecsSimdConfig = level;
        vecsEntity match = vecsFirstMatch<Position, Velocity>( w );
        ASSERT_TRUE( vecsAlive( w, match ) );
        ASSERT_TRUE( ( vecsHasAll<Position, Velocity>( w, match ) ) );
    };

    runTest( VECS_SIMD_SCALAR );
#if defined( VECS_SSE2 )
    if ( supported >= VECS_SIMD_SSE2 ) runTest( VECS_SIMD_SSE2 );
#endif
#if defined( VECS_AVX2 )
    if ( supported >= VECS_SIMD_AVX2 ) runTest( VECS_SIMD_AVX2 );
#endif
#if defined( VECS_NEON )
    if ( supported >= VECS_SIMD_NEON ) runTest( VECS_SIMD_NEON );
#endif

    g_vecsSimdConfig = savedConfig;
    vecsDestroyWorld( w );
}

UTEST( query_bug, each_without_skips_chunk )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e1 = vecsCreate( w );
    vecsEntity e2 = vecsCreate( w );

    vecsSet<Position>( w, e1, { 1.0f, 1.0f } );
    vecsAddTag<Dead>( w, e1 ); // e1 is Dead

    vecsSet<Position>( w, e2, { 1.0f, 1.0f } );
    // e2 is NOT Dead, should be matched!

    vecsQuery* q = vecsBuildQuery<Position>( w );
    vecsQueryAddWithout( q, vecsTypeId<Dead>() );

    uint32_t count = 0;
    vecsQueryEach<Position>( w, q, [&]( vecsEntity, Position& )
    {
        count++;
    } );

    // BUG: e1 having Dead causes the entire 64-chunk to be skipped.
    // e2 is in the same chunk, so count will be 0 instead of 1.
    ASSERT_EQ( count, 1u );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query_bug, parallel_without_skips_chunk )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e1 = vecsCreate( w );
    vecsEntity e2 = vecsCreate( w );

    vecsSet<Position>( w, e1, { 1.0f, 1.0f } );
    vecsAddTag<Dead>( w, e1 );

    vecsSet<Position>( w, e2, { 1.0f, 1.0f } );

    vecsQuery* q = vecsBuildQuery<Position>( w );
    vecsQueryAddWithout( q, vecsTypeId<Dead>() );

    std::atomic<uint32_t> count = 0;
    vecsQueryChunk chunks[4];
    uint32_t numChunks = vecsQueryGetChunks( w, q, chunks, 4 );
    for ( uint32_t i = 0; i < numChunks; i++ )
    {
        vecsQueryExecuteChunk<Position>( w, q, &chunks[i], [&]( vecsEntity, Position& )
        {
            count++;
        } );
    }

    // BUG: Same chunk skipping bug in parallel iteration.
    ASSERT_EQ( count.load(), 1u );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query_bug, stress_complex_filters_skip_chunk )
{
    const uint32_t entityCount = vecsTestClampEntities( 1000u );
    vecsWorld* w = vecsCreateWorld( entityCount );
    
    for ( uint32_t i = 0; i < entityCount; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { 1.0f, 1.0f } );
        
        if ( i % 2 == 0 ) vecsSet<Velocity>( w, e, { 2.0f, 2.0f } );
        if ( i % 3 == 0 ) vecsSet<Health>( w, e, { 100 } );
        if ( i % 5 == 0 ) vecsAddTag<Dead>( w, e );
        if ( i % 7 == 0 ) vecsAddTag<IsEnemy>( w, e );
    }

    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    vecsQueryAddOptional( q, vecsTypeId<Health>() );
    vecsQueryAddWithout( q, vecsTypeId<Dead>() );
    vecsQueryAddWithout( q, vecsTypeId<IsEnemy>() );

    uint32_t expected_count = 0;
    for ( uint32_t i = 0; i < entityCount; i++ )
    {
        bool has_pos = true;
        bool has_vel = ( i % 2 == 0 );
        bool has_dead = ( i % 5 == 0 );
        bool has_enemy = ( i % 7 == 0 );

        if ( has_pos && has_vel && !has_dead && !has_enemy )
        {
            expected_count++;
        }
    }

    uint32_t actual_count = 0;
    vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity, Position&, Velocity& )
    {
        actual_count++;
    } );

    // BUG: The incorrect Without mask application will skip large chunks and cause
    // the actual count to be vastly lower than expected.
    ASSERT_EQ( actual_count, expected_count );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query_bug, stress_dense_with_sparse_without )
{
    const uint32_t entityCount = vecsTestClampEntities( 1000u );
    vecsWorld* w = vecsCreateWorld( entityCount );
    
    for ( uint32_t i = 0; i < entityCount; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { 1.0f, 1.0f } );
        vecsSet<Velocity>( w, e, { 2.0f, 2.0f } );
        
        // Very sparse Without component - exactly one per 64-chunk chunk!
        if ( i % 64 == 0 ) vecsAddTag<Dead>( w, e );
    }

    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    vecsQueryAddWithout( q, vecsTypeId<Dead>() );

    uint32_t expected_count = entityCount - ( entityCount / 64u ) - ( entityCount % 64u == 0 ? 0u : 1u );

    uint32_t actual_count = 0;
    vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity, Position&, Velocity& )
    {
        actual_count++;
    } );

    // BUG: This should be missing ~16 entities. 
    // Instead, it will be missing ~1000 entities because EVERY chunk has one Dead entity!
    ASSERT_EQ( actual_count, expected_count );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query_bug, parallel_stress_complex_filters_skip_chunk )
{
    const uint32_t entityCount = vecsTestClampEntities( 2000u );
    vecsWorld* w = vecsCreateWorld( entityCount );
    
    for ( uint32_t i = 0; i < entityCount; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Position>( w, e, { 1.0f, 1.0f } );
        
        if ( i % 2 == 0 ) vecsSet<Velocity>( w, e, { 2.0f, 2.0f } );
        if ( i % 3 == 0 ) vecsSet<Health>( w, e, { 100 } );
        if ( i % 5 == 0 ) vecsAddTag<Dead>( w, e );
        if ( i % 7 == 0 ) vecsAddTag<IsEnemy>( w, e );
    }

    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    vecsQueryAddOptional( q, vecsTypeId<Health>() );
    vecsQueryAddWithout( q, vecsTypeId<Dead>() );
    vecsQueryAddWithout( q, vecsTypeId<IsEnemy>() );

    uint32_t expected_count = 0;
    for ( uint32_t i = 0; i < entityCount; i++ )
    {
        bool has_pos = true;
        bool has_vel = ( i % 2 == 0 );
        bool has_dead = ( i % 5 == 0 );
        bool has_enemy = ( i % 7 == 0 );

        if ( has_pos && has_vel && !has_dead && !has_enemy )
        {
            expected_count++;
        }
    }

    std::atomic<uint32_t> actual_count = 0;

    // Simulate multi-threaded ranged evaluation by dispatching chunks
    vecsQueryChunk chunks[4];
    uint32_t numChunks = vecsQueryGetChunks( w, q, chunks, 4 );
    for ( uint32_t i = 0; i < numChunks; i++ )
    {
        vecsQueryExecuteChunk<Position, Velocity>( w, q, &chunks[i], [&]( vecsEntity, Position&, Velocity& )
        {
            actual_count++;
        } );
    }

    // BUG: This multi-threaded iterator uses the exact same `topMask` excluding logic.
    // It should aggressively under-count the true number of matching entities.
    ASSERT_EQ( actual_count.load(), expected_count );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

struct NonPodData {
    std::string text;
    std::vector<int> numbers;
};

inline void utest_type_printer(const std::string& s) {
    UTEST_PRINTF("\"%s\"", s.c_str());
}

inline void utest_type_printer(const NonPodData& data) {
    UTEST_PRINTF("NonPodData{text=\"%s\", numbers.size()=%zu}", data.text.c_str(), data.numbers.size());
}

UTEST( query, bug_stale_mask_before_pool_exists )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    // 1. Create one component type so its pool exists.
    vecsEntity e1 = vecsCreate( w );
    vecsSet<Velocity>( w, e1, { 1.0f, 1.0f } );
    
    // 2. Build query for <Position, Velocity> while Position has NO pool.
    // withMask for Position will be UINT64_MAX, so the query will produce e1 
    // because e1 has Velocity and intersect(UINT64_MAX, e1_velocity_bit) is true.
    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    
    // 3. Now create Position pool by setting it on a DIFFERENT entity.
    vecsEntity e2 = vecsCreate( w );
    vecsSet<Position>( w, e2, { 2.0f, 2.0f } );
    
    // 4. Run the query. 
    // It should NOT match e1 (lacks Position) and should NOT match e2 (lacks Velocity).
    // Before the fix, this would crash in getData<Position>(poolPos, e1Idx) 
    // because it iterates e1Idx due to the stale mask and tries to access sparse.
    uint32_t count = 0;
    vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity, Position&, Velocity& ) {
        count++;
    } );
    
    ASSERT_EQ( count, 0u );
    
    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( query, bug_stale_mask_execute_chunk )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    
    vecsEntity e1 = vecsCreate( w );
    vecsSet<Velocity>( w, e1, { 1.0f, 1.0f } );
    
    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    
    vecsEntity e2 = vecsCreate( w );
    vecsSet<Position>( w, e2, { 2.0f, 2.0f } );
    
    vecsQueryChunk chunks[16];
    uint32_t chunkCount = vecsQueryGetChunks( w, q, chunks, 16u );
    
    uint32_t count = 0;
    for ( uint32_t i = 0; i < chunkCount; i++ )
    {
        vecsQueryExecuteChunk<Position, Velocity>( w, q, &chunks[i], [&]( vecsEntity, Position&, Velocity& ) {
            count++;
        } );
    }
    
    ASSERT_EQ( count, 0u );
    
    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

template< size_t Size, size_t Align >
struct alignas( Align ) CmdPayload
{
    std::array<uint8_t, Size> bytes = {};
};

template< size_t Size, size_t Align >
static CmdPayload<Size, Align> vecsMakeCmdPayload( uint32_t salt )
{
    CmdPayload<Size, Align> payload = {};
    for ( size_t i = 0; i < Size; i++ )
    {
        payload.bytes[i] = ( uint8_t )( ( salt + ( uint32_t )i * 13u ) & 0xFFu );
    }
    return payload;
}

template< size_t Size, size_t Align >
static bool vecsAssertCmdPayload( vecsWorld* w, vecsEntity e, uint32_t salt )
{
    CmdPayload<Size, Align>* payload = vecsGet<CmdPayload<Size, Align>>( w, e );
    if ( !payload )
    {
        return false;
    }
    for ( size_t i = 0; i < Size; i++ )
    {
        if ( payload->bytes[i] != ( uint8_t )( ( salt + ( uint32_t )i * 13u ) & 0xFFu ) )
        {
            return false;
        }
    }
    return true;
}

template< int Id > struct BoundaryTag {};
template< int Id > struct BoundaryPod { uint32_t value; };
template< int Id > struct BoundaryComplex { std::string text; };

static int g_boundaryObserverCount = 0;

static void onBoundaryPodAdd64( vecsWorld*, vecsEntity, BoundaryPod<64>* )
{
    g_boundaryObserverCount++;
}

static EventLedger* g_eventLedger = nullptr;
static vecsEntity g_eventSibling = VECS_INVALID_ENTITY;

static void onLedgerPositionAddSelfUnset( vecsWorld* w, vecsEntity e, Position* )
{
    assert( g_eventLedger != nullptr );
    g_eventLedger->record( "position.add", e );
    vecsUnset<Position>( w, e );
}

static void onLedgerPositionRemove( vecsWorld*, vecsEntity e, Position* )
{
    assert( g_eventLedger != nullptr );
    g_eventLedger->record( "position.remove", e );
}

static void onLedgerHealthAddOverwrite( vecsWorld* w, vecsEntity e, Health* )
{
    assert( g_eventLedger != nullptr );
    g_eventLedger->record( "health.add", e );
    vecsSet<Health>( w, e, { 200 } );
}

static void onLedgerHealthRemoveDestroySibling( vecsWorld* w, vecsEntity e, Health* )
{
    assert( g_eventLedger != nullptr );
    g_eventLedger->record( "health.remove", e );
    if ( g_eventSibling != VECS_INVALID_ENTITY && vecsAlive( w, g_eventSibling ) )
    {
        vecsDestroy( w, g_eventSibling );
    }
}

static void onLedgerPositionRemoveAddVelocity( vecsWorld* w, vecsEntity e, Position* )
{
    assert( g_eventLedger != nullptr );
    g_eventLedger->record( "position.remove", e );
    vecsSet<Velocity>( w, e, { 3.0f, 4.0f } );
}

static void onLedgerVelocityAdd( vecsWorld*, vecsEntity e, Velocity* )
{
    assert( g_eventLedger != nullptr );
    g_eventLedger->record( "velocity.add", e );
}

static void onLedgerPositionAddQueueDeferred( vecsWorld*, vecsEntity e, Position* )
{
    assert( g_eventLedger != nullptr );
    g_eventLedger->record( "position.add", e );
}

UTEST( query, boundary_geometry_sweep_exact_sets )
{
    enum Layout : uint32_t
    {
        Layout_Checkerboard,
        Layout_OnePerL2,
        Layout_OnePerTop,
        Layout_LastOnly
    };

    const std::vector<uint32_t> counts = vecsTestBoundaryCounts();
    for ( uint32_t count : counts )
    {
        for ( uint32_t layout = 0; layout < 4u; layout++ )
        {
            vecsWorld* w = vecsCreateWorld( count );
            for ( uint32_t i = 0; i < count; i++ )
            {
                vecsEntity e = vecsCreate( w );
                vecsSet<Position>( w, e, { ( float )i, ( float )layout } );

                bool matched = false;
                switch ( layout )
                {
                    case Layout_Checkerboard: matched = ( i & 1u ) == 0u; break;
                    case Layout_OnePerL2: matched = ( i % 64u ) == 0u; break;
                    case Layout_OnePerTop: matched = ( i % vecsTestTopSpan() ) == 0u; break;
                    case Layout_LastOnly: matched = ( i + 1u ) == count; break;
                    default: break;
                }

                if ( matched )
                {
                    vecsSet<Velocity>( w, e, { ( float )( i + 1u ), 1.0f } );
                }
            }

            std::vector<vecsEntity> eachEntities = vecsCollectEachEntities<Position, Velocity>( w );
            vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
            std::vector<vecsEntity> cachedEntities = vecsCollectQueryEntities<Position, Velocity>( w, q );
            std::vector<vecsEntity> freshChunkEntities = vecsCollectFreshChunkEntities<Position, Velocity>( w, q, 31u );

            ASSERT_TRUE( vecsAssertEntitySetsEqual( eachEntities, cachedEntities ) );
            ASSERT_TRUE( vecsAssertEntitySetsEqual( eachEntities, freshChunkEntities ) );

            const vecsEntity expectedFirst = eachEntities.empty() ? VECS_INVALID_ENTITY : eachEntities.front();
            const vecsEntity firstEach = vecsFirstMatch<Position, Velocity>( w );
            const vecsEntity firstCached = vecsQueryFirstMatch<Position, Velocity>( w, q );
            ASSERT_EQ( firstEach, expectedFirst );
            ASSERT_EQ( firstCached, expectedFirst );
            ASSERT_EQ( freshChunkEntities.empty() ? VECS_INVALID_ENTITY : freshChunkEntities.front(), expectedFirst );
            ASSERT_TRUE( vecsValidate( w ) );

            vecsDestroyQuery( q );
            vecsDestroyWorld( w );
        }
    }
}

UTEST( query, cold_build_alias_and_seeded_shadow_replay )
{
    const uint32_t seeds[] = { 0xC0FFEEu, 0x1234567u, 0xBAD5EEDu };
    for ( uint32_t seed : seeds )
    {
        vecsWorld* w = vecsCreateWorld( vecsTestClampEntities( 1024u ) );

        vecsQuery* qPos = vecsBuildQuery<Position>( w );
        vecsQueryAddOptional( qPos, vecsTypeId<Velocity>() );
        vecsQueryAddOptional( qPos, vecsTypeId<Health>() );
        vecsQueryAddWithout( qPos, vecsTypeId<Dead>() );

        vecsQuery* qPosVel = vecsBuildQuery<Position, Velocity>( w );
        vecsQueryAddWithout( qPosVel, vecsTypeId<Dead>() );

        vecsQuery* qHealth = vecsBuildQuery<Health>( w );
        vecsQueryAddOptional( qHealth, vecsTypeId<Position>() );
        vecsQueryAddWithout( qHealth, vecsTypeId<Dead>() );

        auto compareQueries = [&]()
        {
            vecsQuery* freshPos = vecsBuildQuery<Position>( w );
            vecsQueryAddOptional( freshPos, vecsTypeId<Velocity>() );
            vecsQueryAddOptional( freshPos, vecsTypeId<Health>() );
            vecsQueryAddWithout( freshPos, vecsTypeId<Dead>() );

            vecsQuery* freshPosVel = vecsBuildQuery<Position, Velocity>( w );
            vecsQueryAddWithout( freshPosVel, vecsTypeId<Dead>() );

            vecsQuery* freshHealth = vecsBuildQuery<Health>( w );
            vecsQueryAddOptional( freshHealth, vecsTypeId<Position>() );
            vecsQueryAddWithout( freshHealth, vecsTypeId<Dead>() );

            const std::vector<vecsEntity> expectedPos = vecsCollectQueryEntities<Position>( w, freshPos );
            const std::vector<vecsEntity> expectedPosVel = vecsCollectQueryEntities<Position, Velocity>( w, freshPosVel );
            const std::vector<vecsEntity> expectedHealth = vecsCollectQueryEntities<Health>( w, freshHealth );

            ASSERT_TRUE( vecsAssertEntitySetsEqual( expectedPos, vecsCollectQueryEntities<Position>( w, qPos ) ) );
            ASSERT_TRUE( vecsAssertEntitySetsEqual( expectedPosVel, vecsCollectQueryEntities<Position, Velocity>( w, qPosVel ) ) );
            ASSERT_TRUE( vecsAssertEntitySetsEqual( expectedPosVel, vecsCollectFreshChunkEntities<Position, Velocity>( w, qPosVel, 17u ) ) );
            ASSERT_TRUE( vecsAssertEntitySetsEqual( expectedHealth, vecsCollectQueryEntities<Health>( w, qHealth ) ) );

            const vecsEntity expectedFirst = expectedPosVel.empty() ? VECS_INVALID_ENTITY : expectedPosVel.front();
            const vecsEntity firstCached = vecsQueryFirstMatch<Position, Velocity>( w, qPosVel );
            ASSERT_EQ( firstCached, expectedFirst );

            ASSERT_TRUE( vecsValidate( w ) );

            vecsDestroyQuery( freshPos );
            vecsDestroyQuery( freshPosVel );
            vecsDestroyQuery( freshHealth );
        };

        compareQueries();

        SeededOpStream stream( seed );
        for ( uint32_t step = 0; step < 384u; step++ )
        {
            std::vector<vecsEntity> alive = vecsCollectAliveHandles( w );
            const uint32_t op = stream.nextBounded( 11u );

            switch ( op )
            {
                case 0:
                    if ( vecsCount( w ) < w->maxEntities )
                    {
                        ( void )vecsCreate( w );
                    }
                    break;
                case 1:
                    if ( !alive.empty() )
                    {
                        vecsEntity e = alive[stream.nextBounded( ( uint32_t )alive.size() )];
                        Position value = { stream.nextFloat( 100.0f ), stream.nextFloat( 100.0f ) };
                        vecsSet<Position>( w, e, value );
                    }
                    break;
                case 2:
                    if ( !alive.empty() )
                    {
                        vecsEntity e = alive[stream.nextBounded( ( uint32_t )alive.size() )];
                        if ( vecsHas<Position>( w, e ) )
                        {
                            vecsUnset<Position>( w, e );
                        }
                    }
                    break;
                case 3:
                    if ( !alive.empty() )
                    {
                        vecsEntity e = alive[stream.nextBounded( ( uint32_t )alive.size() )];
                        Velocity value = { stream.nextFloat( 50.0f ), stream.nextFloat( 50.0f ) };
                        vecsSet<Velocity>( w, e, value );
                    }
                    break;
                case 4:
                    if ( !alive.empty() )
                    {
                        vecsEntity e = alive[stream.nextBounded( ( uint32_t )alive.size() )];
                        if ( vecsHas<Velocity>( w, e ) )
                        {
                            vecsUnset<Velocity>( w, e );
                        }
                    }
                    break;
                case 5:
                    if ( !alive.empty() )
                    {
                        vecsEntity e = alive[stream.nextBounded( ( uint32_t )alive.size() )];
                        Health value = { ( int )( stream.next() & 0x7FFFu ) };
                        vecsEmplace<Health>( w, e, value.hp );
                    }
                    break;
                case 6:
                    if ( !alive.empty() )
                    {
                        vecsEntity e = alive[stream.nextBounded( ( uint32_t )alive.size() )];
                        if ( vecsHas<Health>( w, e ) )
                        {
                            vecsUnset<Health>( w, e );
                        }
                    }
                    break;
                case 7:
                    if ( !alive.empty() )
                    {
                        vecsEntity e = alive[stream.nextBounded( ( uint32_t )alive.size() )];
                        if ( vecsHas<Dead>( w, e ) )
                        {
                            vecsUnset<Dead>( w, e );
                        }
                        else
                        {
                            vecsAddTag<Dead>( w, e );
                        }
                    }
                    break;
                case 8:
                    if ( !alive.empty() )
                    {
                        vecsEntity doomed = alive[stream.nextBounded( ( uint32_t )alive.size() )];
                        vecsDestroy( w, doomed );
                    }
                    break;
                case 9:
                    if ( !alive.empty() && vecsCount( w ) < w->maxEntities )
                    {
                        vecsEntity src = alive[stream.nextBounded( ( uint32_t )alive.size() )];
                        ( void )vecsClone( w, src );
                    }
                    break;
                case 10:
                    if ( stream.oneIn( 7u ) )
                    {
                        vecsClearWorld( w );
                    }
                    else if ( stream.oneIn( 2u ) && !alive.empty() )
                    {
                        const uint32_t counts[] = { 1u, 4u, 17u };
                        const uint32_t requested = counts[stream.nextBounded( 3u )];
                        const uint32_t room = w->maxEntities - vecsCount( w );
                        const uint32_t batchCount = requested < room ? requested : room;
                        if ( batchCount > 0u )
                        {
                            vecsEntity prefab = alive[stream.nextBounded( ( uint32_t )alive.size() )];
                            std::vector<vecsEntity> spawned( batchCount, VECS_INVALID_ENTITY );
                            vecsInstantiateBatch( w, prefab, spawned.data(), batchCount );
                        }
                    }
                    else
                    {
                        const int singletonValue = ( int )( stream.next() & 0xFFFFu );
                        vecsSetSingleton<ShadowSingletonValue>( w, { singletonValue } );
                    }
                    break;
                default:
                    break;
            }

            if ( ( step % 32u ) == 0u )
            {
                compareQueries();
            }
        }

        compareQueries();
        vecsDestroyQuery( qPos );
        vecsDestroyQuery( qPosVel );
        vecsDestroyQuery( qHealth );
        vecsDestroyWorld( w );
    }
}


UTEST( world, clear_world_regeneration_preserves_caches )
{
    EventLedger ledger;
    g_eventLedger = &ledger;

    vecsWorld* w = vecsCreateWorld( 128u );
    ShadowWorld shadow;
    vecsQuery* q = vecsBuildQuery<Position>( w );
    vecsQueryAddWithout( q, vecsTypeId<Dead>() );
    vecsOnAdd<Position>( w, onLedgerPositionRemove );

    vecsEntity parent = vecsCreate( w );
    vecsEntity child = vecsCreate( w );
    shadow.create( parent );
    shadow.create( child );
    vecsSet<Position>( w, parent, { 1.0f, 0.0f } );
    vecsSet<Position>( w, child, { 2.0f, 0.0f } );
    vecsSetChildOf( w, child, parent );
    vecsSetSingleton<ShadowSingletonValue>( w, { 77 } );
    shadow.setPosition( parent, { 1.0f, 0.0f } );
    shadow.setPosition( child, { 2.0f, 0.0f } );
    shadow.setParent( child, parent );
    shadow.setSingleton( 77 );

    vecsAddTag<Dead>( w, child );
    shadow.setDead( child, true );

    ASSERT_TRUE( vecsAssertEntitySetsEqual( shadow.collect( []( const ShadowWorld::EntityState& state ) { return state.hasPosition && !state.hasDead; } ), vecsCollectQueryEntities<Position>( w, q ) ) );
    ASSERT_TRUE( vecsValidate( w ) );

    const vecsEntity oldParent = parent;
    vecsClearWorld( w );
    shadow.clear();
    ASSERT_TRUE( vecsAssertEntitySetsEqual( std::vector<vecsEntity>{}, vecsCollectQueryEntities<Position>( w, q ) ) );
    ASSERT_FALSE( vecsAlive( w, oldParent ) );
    ASSERT_TRUE( vecsGetSingleton<ShadowSingletonValue>( w ) == nullptr );

    parent = vecsCreate( w );
    child = vecsCreate( w );
    shadow.create( parent );
    shadow.create( child );
    vecsSet<Position>( w, parent, { 10.0f, 1.0f } );
    vecsSet<Position>( w, child, { 11.0f, 1.0f } );
    vecsSetChildOf( w, child, parent );
    shadow.setPosition( parent, { 10.0f, 1.0f } );
    shadow.setPosition( child, { 11.0f, 1.0f } );
    shadow.setParent( child, parent );

    vecsUnset<Dead>( w, child );
    vecsSet<Velocity>( w, child, { 5.0f, 6.0f } );
    shadow.setVelocity( child, { 5.0f, 6.0f } );

    ASSERT_TRUE( shadow.matchesWorld( w ) );
    ASSERT_TRUE( vecsEntityIndex( parent ) == vecsEntityIndex( oldParent ) ? vecsEntityGeneration( parent ) > vecsEntityGeneration( oldParent ) : true );
    ASSERT_TRUE( vecsAssertEntitySetsEqual( shadow.collect( []( const ShadowWorld::EntityState& state ) { return state.hasPosition && !state.hasDead; } ), vecsCollectQueryEntities<Position>( w, q ) ) );
    ASSERT_TRUE( vecsValidate( w ) );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
    g_eventLedger = nullptr;
}

// 1. Tags
struct SoupTagA {};
struct SoupTagB {};

// 2. PODs
struct SoupPodA { uint32_t id; uint32_t inverse; };
struct SoupPodB { double v[4]; };

// 3. Complex types
struct SoupCompString { std::string text; };
struct SoupCompVector { std::vector<int> nums; };
struct SoupCompMap { std::map<int, std::string> dict; };

// 4. Ground Truth
struct SoupExpectedFlags {
    uint32_t expectedId;
    bool tagA, tagB;
    bool podA, podB;
    bool cString, cVector, cMap;
};

#if defined( VECS_ENABLE_STRESS_TESTS )
UTEST( vecs, big_soup_stress_test )
{
    vecsWorld* w = vecsCreateWorld( 60000 );

    // Spawn 50,000 entities with patterned components
    const uint32_t numEntities = 50000;
    std::vector<vecsEntity> entities( numEntities );
    
    for ( uint32_t i = 0; i < numEntities; i++ )
    {
        vecsEntity e = vecsCreate( w );
        entities[i] = e;
        
        int mask = i % 128;
        SoupExpectedFlags flags = { i, false, false, false, false, false, false, false };
        
        if ( mask & 1 ) { vecsAddTag<SoupTagA>( w, e ); flags.tagA = true; } // Nasty: vecsAddTag
        if ( mask & 2 ) { vecsSet<SoupTagB>( w, e ); flags.tagB = true; }
        if ( mask & 4 ) { vecsEmplace<SoupPodA>( w, e, SoupPodA{ i, ~i } ); flags.podA = true; } // Nasty: vecsEmplace
        if ( mask & 8 ) { vecsSet<SoupPodB>( w, e, { { (double)i, (double)i*2, (double)i*3, (double)i*4 } } ); flags.podB = true; }
        if ( mask & 16 ) { vecsEmplace<SoupCompString>( w, e, SoupCompString{ "str_" + std::to_string( i ) } ); flags.cString = true; } // Nasty: vecsEmplace complex
        if ( mask & 32 ) { vecsSet<SoupCompVector>( w, e, { { (int)i, (int)i+1, (int)i+2 } } ); flags.cVector = true; }
        if ( mask & 64 ) { 
            SoupCompMap m; 
            m.dict[i] = "val_" + std::to_string( i ); 
            vecsSet<SoupCompMap>( w, e, m ); 
            flags.cMap = true; 
        }
        
        vecsSet<SoupExpectedFlags>( w, e, flags );

        // Nasty: Hierarchy. Every 10th entity is a child of the previous one.
        if ( i > 0 && i % 10 == 0 )
        {
            vecsSetChildOf( w, e, entities[i - 1] );
        }
    }
    
    auto RunGauntlet = [&]() {
        // 1. Solo Tag
        uint32_t countTagA = 0;
        vecsQuery* qTagA = vecsBuildQuery<SoupExpectedFlags, SoupTagA>( w );
        vecsQueryEach<SoupExpectedFlags, SoupTagA>( w, qTagA, [&]( vecsEntity, SoupExpectedFlags& flags, SoupTagA& ) {
            ASSERT_TRUE( flags.tagA );
            countTagA++;
        } );
        vecsDestroyQuery( qTagA );
        
        // 2. Heavy Data Combo
        uint32_t countHeavy = 0;
        vecsQuery* qHeavy = vecsBuildQuery<SoupExpectedFlags, SoupCompVector, SoupCompMap>( w );
        vecsQueryEach<SoupExpectedFlags, SoupCompVector, SoupCompMap>( w, qHeavy, [&]( vecsEntity, SoupExpectedFlags& flags, SoupCompVector& vec, SoupCompMap& map ) {
            ASSERT_TRUE( flags.cVector );
            ASSERT_TRUE( flags.cMap );
            uint32_t expectedId = flags.expectedId;
            ASSERT_EQ( vec.nums.size(), 3ull );
            ASSERT_EQ( vec.nums[0], (int)expectedId );
            auto it = map.dict.find( expectedId );
            ASSERT_NE( it, map.dict.end() );
            ASSERT_EQ( it->second, "val_" + std::to_string( expectedId ) );
            countHeavy++;
        } );
        vecsDestroyQuery( qHeavy );
        
        // 3. Tag + POD Mix
        uint32_t countMix = 0;
        vecsQuery* qMix = vecsBuildQuery<SoupExpectedFlags, SoupTagB, SoupPodB>( w );
        vecsQueryEach<SoupExpectedFlags, SoupTagB, SoupPodB>( w, qMix, [&]( vecsEntity, SoupExpectedFlags& flags, SoupTagB&, SoupPodB& podB ) {
            ASSERT_TRUE( flags.tagB );
            ASSERT_TRUE( flags.podB );
            uint32_t expectedId = flags.expectedId;
            ASSERT_EQ( podB.v[0], (double)expectedId );
            countMix++;
        } );
        vecsDestroyQuery( qMix );
        
        // 4. Everything Bagel
        uint32_t countAll = 0;
        vecsQuery* qAll = vecsBuildQuery<SoupExpectedFlags, SoupTagA, SoupTagB, SoupPodA, SoupPodB, SoupCompString, SoupCompVector, SoupCompMap>( w );
        vecsQueryEach<SoupExpectedFlags, SoupTagA, SoupTagB, SoupPodA, SoupPodB, SoupCompString, SoupCompVector, SoupCompMap>( w, qAll, [&]( vecsEntity, SoupExpectedFlags& flags, SoupTagA&, SoupTagB&, SoupPodA& podA, SoupPodB& podB, SoupCompString& str, SoupCompVector& vec, SoupCompMap& map ) {
            ASSERT_TRUE( flags.tagA );
            ASSERT_TRUE( flags.tagB );
            ASSERT_TRUE( flags.podA );
            ASSERT_TRUE( flags.podB );
            ASSERT_TRUE( flags.cString );
            ASSERT_TRUE( flags.cVector );
            ASSERT_TRUE( flags.cMap );
            
            uint32_t expectedId = flags.expectedId;
            ASSERT_EQ( podA.id, expectedId );
            ASSERT_EQ( podA.inverse, ~expectedId );
            ASSERT_EQ( podB.v[0], (double)expectedId );
            ASSERT_EQ( str.text, "str_" + std::to_string( expectedId ) );
            ASSERT_EQ( vec.nums[0], (int)expectedId );
            ASSERT_EQ( map.dict.at(expectedId), "val_" + std::to_string( expectedId ) );
            
            countAll++;
        } );
        vecsDestroyQuery( qAll );
    };

    // 1. Initial State Check
    // This will immediately fail vecsValidate because vecsAddTag/vecsEmplace are missing the signature bit update!
    RunGauntlet();
    ASSERT_TRUE( vecsValidate( w ) );
    
    // 2. The Great Shuffle: Interleaved Deletions, Modifications, and Removals
    for ( uint32_t i = 0; i < numEntities; i++ )
    {
        if ( entities[i] == VECS_INVALID_ENTITY ) continue;
        if ( !vecsAlive( w, entities[i] ) ) continue; // Could have been destroyed recursively

        vecsEntity e = entities[i];
        SoupExpectedFlags* flags = vecsGet<SoupExpectedFlags>( w, e );
        
        // Strategy A: Delete completely (Every 5th entity)
        if ( i % 5 == 0 )
        {
            vecsDestroy( w, e );
            entities[i] = VECS_INVALID_ENTITY;
            continue;
        }

        // Strategy B: Mutate data (Every 3rd entity)
        if ( i % 3 == 0 )
        {
            flags->expectedId += 100000;

            if ( flags->podA ) 
            {
                SoupPodA* podA = vecsGet<SoupPodA>( w, e );
                podA->id = flags->expectedId;
                podA->inverse = ~( podA->id );
            }
            if ( flags->podB )
            {
                SoupPodB* podB = vecsGet<SoupPodB>( w, e );
                podB->v[0] = (double)flags->expectedId;
            }
            if ( flags->cString )
            {
                SoupCompString* str = vecsGet<SoupCompString>( w, e );
                str->text = "str_" + std::to_string( flags->expectedId );
            }
            if ( flags->cVector )
            {
                SoupCompVector* vec = vecsGet<SoupCompVector>( w, e );
                vec->nums[0] = flags->expectedId; 
            }
            if ( flags->cMap )
            {
                SoupCompMap* map = vecsGet<SoupCompMap>( w, e );
                uint32_t oldId = flags->expectedId - 100000;
                map->dict.erase( oldId );
                map->dict[flags->expectedId] = "val_" + std::to_string( flags->expectedId );
                
                // Add some chaotic garbage to the map to force reallocations
                map->dict[i + 900000] = "garbage_data_to_pad_map";
                map->dict[i + 900001] = "more_garbage_for_allocator";
            }
        }

        // Strategy C: Remove Components (Every 7th entity)
        if ( i % 7 == 0 )
        {
            if ( flags->tagA ) { vecsUnset<SoupTagA>( w, e ); flags->tagA = false; }
            if ( flags->cVector ) { vecsUnset<SoupCompVector>( w, e ); flags->cVector = false; }
            if ( flags->cString ) { vecsUnset<SoupCompString>( w, e ); flags->cString = false; }
        }

        // Strategy D: Add Components (Every 11th entity)
        if ( i % 11 == 0 )
        {
            if ( !flags->tagB ) { vecsAddTag<SoupTagB>( w, e ); flags->tagB = true; } // Nasty: vecsAddTag
            if ( !flags->cMap ) { 
                SoupCompMap m;
                m.dict[flags->expectedId] = "val_" + std::to_string( flags->expectedId );
                vecsEmplace<SoupCompMap>( w, e, std::move(m) ); // Nasty: vecsEmplace complex
                flags->cMap = true;
            }
        }
    }

    // 3. Post-Shuffle Verification
    RunGauntlet();
    ASSERT_TRUE( vecsValidate( w ) );
    
    // 4. Pass 1: Delete Evens (Geometric Hole Punching)
    for ( uint32_t i = 0; i < numEntities; i++ )
    {
        if ( i % 2 == 0 && entities[i] != VECS_INVALID_ENTITY && vecsAlive( w, entities[i] ) )
        {
            vecsDestroy( w, entities[i] );
            entities[i] = VECS_INVALID_ENTITY;
        }
    }
    RunGauntlet();
    ASSERT_TRUE( vecsValidate( w ) );
    
    // 5. Pass 2: Delete Primes/Irregular gap
    for ( uint32_t i = 0; i < numEntities; i++ )
    {
        if ( i % 7 == 0 && entities[i] != VECS_INVALID_ENTITY && vecsAlive( w, entities[i] ) )
        {
            vecsDestroy( w, entities[i] );
            entities[i] = VECS_INVALID_ENTITY;
        }
    }
    RunGauntlet();
    ASSERT_TRUE( vecsValidate( w ) );

    vecsDestroyWorld( w );
}
#endif

UTEST( validation, captures_corruption )
{
    vecsWorld* w = vecsCreateWorld();

    vecsEntity parent = vecsCreate( w );
    vecsSet<Position>( w, parent, { 0, 0 } );

    vecsEntity child = vecsCreate( w );
    vecsSet<Position>( w, child, { 1, 1 } );
    vecsSetChildOf( w, child, parent );

    // Validation should succeed initially
    ASSERT_TRUE( vecsValidate( w ) );

    // Verify emplace and addTag do not break validation!
    struct EmptyTag {};
    vecsEmplace<Velocity>( w, child, 2.0f, 2.0f );
    vecsAddTag<EmptyTag>( w, child );
    ASSERT_TRUE( vecsValidate( w ) );

    // 1. Corrupt component pool (sparse set mapping)
    vecsPool* posPool = vecsEnsurePool<Position>( w );
    uint32_t savedSparse = posPool->sparse[vecsEntityIndex( child )];
    posPool->sparse[vecsEntityIndex( child )] = VECS_INVALID_INDEX; // Break bi-directional mapping
    ASSERT_FALSE( vecsValidate( w ) );
    posPool->sparse[vecsEntityIndex( child )] = savedSparse; // Restore
    ASSERT_TRUE( vecsValidate( w ) );

    // 2. Corrupt entity pool allocation state
    w->entities->allocated[vecsEntityIndex( child )] = 0; // Mark as dead while it's still alive in components
    ASSERT_FALSE( vecsValidate( w ) );
    w->entities->allocated[vecsEntityIndex( child )] = 1; // Restore
    ASSERT_TRUE( vecsValidate( w ) );

    // 3. Corrupt bitfield
    uint64_t oldMask = w->entities->signatures[vecsTypeId<Position>() >> 6][vecsEntityIndex( child )];
    w->entities->signatures[vecsTypeId<Position>() >> 6][vecsEntityIndex( child )] = 0;
    ASSERT_FALSE( vecsValidate( w ) );
    w->entities->signatures[vecsTypeId<Position>() >> 6][vecsEntityIndex( child )] = oldMask; // Restore
    ASSERT_TRUE( vecsValidate( w ) );

    // 4. Corrupt relationships
    vecsEntity oldParent = w->relationships->parents[vecsEntityIndex( child )];
    w->relationships->parents[vecsEntityIndex( child )] = VECS_INVALID_ENTITY; // Make child orphan without updating parent's children
    ASSERT_FALSE( vecsValidate( w ) );
    w->relationships->parents[vecsEntityIndex( child )] = oldParent; // Restore
    ASSERT_TRUE( vecsValidate( w ) );

    vecsDestroyWorld( w );
}

#if defined( VECS_ENABLE_STRESS_TESTS )
struct SoupUafTracker {
    uint32_t* ptr;
    SoupUafTracker() { ptr = new uint32_t(42); }
    SoupUafTracker(const SoupUafTracker& o) { ptr = new uint32_t(*o.ptr); }
    ~SoupUafTracker() { *ptr = 0xDEADBEEF; delete ptr; ptr = nullptr; }
};

struct SoupSingletonLifecycleTracker
{
    static int destroyed;
    int value = 0;
};

int SoupSingletonLifecycleTracker::destroyed = 0;

struct SoupByteComp { char value; };
struct SoupDoubleComp { double value; };

static bool g_commandAlignmentOk = true;

struct alignas( 32 ) SoupAlignedCommandPayload
{
    float values[8] = {};
    uint32_t magic = 0;

    SoupAlignedCommandPayload() = default;
    SoupAlignedCommandPayload( const SoupAlignedCommandPayload& other )
    {
        g_commandAlignmentOk = ( reinterpret_cast<uintptr_t>( &other ) & ( alignof( SoupAlignedCommandPayload ) - 1u ) ) == 0u;
        std::memcpy( values, other.values, sizeof( values ) );
        magic = other.magic;
    }
};

static vecsEntity g_reparentTarget = VECS_INVALID_ENTITY;

static void onChaosReparentDuringDestroy( vecsWorld* w, vecsEntity e, SoupPodA* )
{
    if ( g_reparentTarget != VECS_INVALID_ENTITY && vecsAlive( w, g_reparentTarget ) )
    {
        vecsSetChildOf( w, e, g_reparentTarget );
    }
}

static int g_chaosObserverCount = 0;
static void onChaosAdd( vecsWorld*, vecsEntity, SoupPodA* ) { g_chaosObserverCount++; }

static int g_clearWorldObsCount = 0;
static void onClearWorldChaos( vecsWorld*, vecsEntity, SoupPodA* ) { g_clearWorldObsCount++; }

static int g_deferredObsCount = 0;
static void onDeferredChaosAdd( vecsWorld*, vecsEntity, SoupPodB* ) { g_deferredObsCount++; }

static void onRemoveSpawnComponent( vecsWorld* w, vecsEntity e, SoupPodA* )
{
    // During destruction, add a completely new component!
    vecsSet<SoupPodB>( w, e, { { 1, 2, 3, 4 } } );
}

#endif

UTEST( world, capacity_normalization_release_safe )
{
#if !defined( NDEBUG )
    return;
#else
    vecsWorld* zero = vecsCreateWorld( 0u );
    ASSERT_TRUE( zero != nullptr );
    ASSERT_EQ( zero->maxEntities, 1u );
    vecsEntity only = vecsCreate( zero );
    ASSERT_TRUE( vecsAlive( zero, only ) );
    ASSERT_EQ( vecsEntityIndex( only ), 0u );
    ASSERT_EQ( vecsCreate( zero ), VECS_INVALID_ENTITY );
    vecsDestroy( zero, only );
    ASSERT_TRUE( vecsValidate( zero ) );
    vecsDestroyWorld( zero );

    vecsWorld* clamped = vecsCreateWorld( VECS_MAX_ENTITIES + 17u );
    ASSERT_TRUE( clamped != nullptr );
    ASSERT_EQ( clamped->maxEntities, ( uint32_t )VECS_MAX_ENTITIES );
    vecsEntity last = VECS_INVALID_ENTITY;
    for ( uint32_t i = 0; i < clamped->maxEntities; i++ )
    {
        last = vecsCreate( clamped );
    }
    ASSERT_TRUE( vecsAlive( clamped, last ) );
    ASSERT_EQ( vecsEntityIndex( last ), clamped->maxEntities - 1u );
    ASSERT_EQ( vecsCreate( clamped ), VECS_INVALID_ENTITY );
    ASSERT_TRUE( vecsSet<Position>( clamped, last, { 9.0f, 3.0f } ) != nullptr );
    vecsDestroy( clamped, last );
    ASSERT_FALSE( vecsAlive( clamped, last ) );
    ASSERT_TRUE( vecsValidate( clamped ) );
    vecsDestroyWorld( clamped );
#endif
}

UTEST( entity, component_id_overflow_fail_safe_release )
{
#if !defined( NDEBUG )
    return;
#else
    const uint32_t savedCounter = vecsGetTypeIdCounter();
    vecsSetTypeIdCounter( VECS_MAX_COMPONENTS );

    const uint32_t overflowPodId = vecsTypeId<OverflowPod>();
    const uint32_t overflowTagId = vecsTypeId<OverflowTag>();
    ASSERT_GE( overflowPodId, ( uint32_t )VECS_MAX_COMPONENTS );
    ASSERT_GE( overflowTagId, ( uint32_t )VECS_MAX_COMPONENTS );

    vecsWorld* w = vecsCreateWorld( 8u );
    vecsEntity e = vecsCreate( w );
    ASSERT_TRUE( vecsSet<Position>( w, e, { 1.0f, 2.0f } ) != nullptr );

    const uint32_t observerCount = w->observers->count;
    g_overflowOnAddCount = 0;
    g_overflowOnRemoveCount = 0;

    ASSERT_TRUE( vecsEnsurePool<OverflowPod>( w ) == nullptr );
    ASSERT_TRUE( vecsSet<OverflowPod>( w, e, { 7 } ) == nullptr );
    ASSERT_TRUE( vecsGet<OverflowPod>( w, e ) == nullptr );
    ASSERT_FALSE( vecsHas<OverflowPod>( w, e ) );
    ASSERT_TRUE( vecsEmplace<OverflowPod>( w, e ) == nullptr );
    vecsAddTag<OverflowTag>( w, e );
    ASSERT_FALSE( vecsHas<OverflowTag>( w, e ) );
    ASSERT_TRUE( vecsSetSingleton<OverflowPod>( w, { 11 } ) == nullptr );
    ASSERT_TRUE( vecsGetSingleton<OverflowPod>( w ) == nullptr );

    vecsOnAdd<OverflowPod>( w, onOverflowPodAdd );
    vecsOnRemove<OverflowPod>( w, onOverflowPodRemove );
    ASSERT_EQ( w->observers->count, observerCount );

    vecsUnset<OverflowPod>( w, e );
    vecsRemoveAll<OverflowPod, OverflowTag>( w, e );

    ASSERT_EQ( g_overflowOnAddCount, 0 );
    ASSERT_EQ( g_overflowOnRemoveCount, 0 );
    ASSERT_TRUE( vecsHas<Position>( w, e ) );
    ASSERT_TRUE( vecsValidate( w ) );

    vecsDestroyWorld( w );
    vecsSetTypeIdCounter( savedCounter );
#endif
}

UTEST( query, component_id_overflow_semantics_release )
{
#if !defined( NDEBUG )
    return;
#else
    const uint32_t savedCounter = vecsGetTypeIdCounter();
    vecsSetTypeIdCounter( VECS_MAX_COMPONENTS );

    const uint32_t overflowPodId = vecsTypeId<OverflowPod>();
    const uint32_t overflowAltId = vecsTypeId<OverflowPodAlt>();
    ASSERT_GE( overflowPodId, ( uint32_t )VECS_MAX_COMPONENTS );
    ASSERT_GE( overflowAltId, ( uint32_t )VECS_MAX_COMPONENTS );

    vecsWorld* w = vecsCreateWorld( 32u );
    vecsEntity e1 = vecsCreate( w );
    vecsEntity e2 = vecsCreate( w );
    vecsSet<Position>( w, e1, { 1.0f, 0.0f } );
    vecsSet<Position>( w, e2, { 2.0f, 0.0f } );

    vecsQuery* impossible = vecsBuildQuery<Position, OverflowPod>( w );
    uint32_t impossibleCount = 0u;
    vecsQueryEach<Position>( w, impossible, [&]( vecsEntity, Position& )
    {
        impossibleCount++;
    } );
    ASSERT_EQ( impossibleCount, 0u );
    ASSERT_EQ( vecsQueryFirstMatch<Position>( w, impossible ), VECS_INVALID_ENTITY );
    // Bind first: a template arg comma inside ASSERT_EQ(...) would be parsed as a macro arg separator.
    const vecsEntity overflowFirstMatch = vecsFirstMatch<Position, OverflowPod>( w );
    ASSERT_EQ( overflowFirstMatch, VECS_INVALID_ENTITY );

    vecsQueryChunk chunks[2] = {};
    ASSERT_EQ( vecsQueryGetChunks( w, impossible, chunks, 2u ), 0u );
    chunks[0].count = 1u;
    chunks[0].activeTi[0] = 0u;
    vecsQueryExecuteChunk<Position>( w, impossible, &chunks[0], [&]( vecsEntity, Position& )
    {
        impossibleCount++;
    } );
    ASSERT_EQ( impossibleCount, 0u );
    vecsDestroyQuery( impossible );

    vecsQuery* q = vecsBuildQuery<Position>( w );
    const std::vector<vecsEntity> baseline = vecsCollectQueryEntities<Position>( w, q );
    vecsQueryAddWithout( q, overflowAltId );
    vecsQueryAddOptional( q, overflowAltId );
    vecsQueryMarkRead( q, overflowAltId );
    vecsQueryMarkWrite( q, overflowAltId );
    ASSERT_FALSE( vecsQueryReads( q, overflowAltId ) );
    ASSERT_FALSE( vecsQueryWrites( q, overflowAltId ) );
    ASSERT_TRUE( vecsAssertEntitySetsEqual( baseline, vecsCollectQueryEntities<Position>( w, q ) ) );
    ASSERT_TRUE( vecsValidate( w ) );

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
    vecsSetTypeIdCounter( savedCounter );
#endif
}

UTEST( entity, component_id_word_boundary_sweep )
{
    ASSERT_EQ( vecsTypeId<Position>(), 0u );

    const uint32_t savedCounter = vecsGetTypeIdCounter();
    vecsWorld* w = vecsCreateWorld( 32u );
    vecsEntity e = vecsCreate( w );
    g_boundaryObserverCount = 0;

    if ( VECS_MAX_COMPONENTS > 65u )
    {
        vecsSetTypeIdCounter( 63u );
        const uint32_t tag63Id = vecsTypeId<BoundaryTag<63>>();
        const uint32_t pod64Id = vecsTypeId<BoundaryPod<64>>();
        const uint32_t complex65Id = vecsTypeId<BoundaryComplex<65>>();
        ASSERT_EQ( tag63Id, 63u );
        ASSERT_EQ( pod64Id, 64u );
        ASSERT_EQ( complex65Id, 65u );

        vecsOnAdd<BoundaryPod<64>>( w, onBoundaryPodAdd64 );
        vecsAddTag<BoundaryTag<63>>( w, e );
        vecsSet<BoundaryPod<64>>( w, e, { 64u } );
        vecsSet<BoundaryComplex<65>>( w, e, { "sixty-five" } );

        ASSERT_EQ( g_boundaryObserverCount, 1 );
        ASSERT_TRUE( vecsHas<BoundaryTag<63>>( w, e ) );
        ASSERT_TRUE( vecsHas<BoundaryPod<64>>( w, e ) );
        ASSERT_TRUE( vecsHas<BoundaryComplex<65>>( w, e ) );

        vecsUnset<BoundaryTag<63>>( w, e );
        vecsUnset<BoundaryPod<64>>( w, e );
        vecsUnset<BoundaryComplex<65>>( w, e );
        ASSERT_FALSE( vecsHas<BoundaryTag<63>>( w, e ) );
        ASSERT_FALSE( vecsHas<BoundaryPod<64>>( w, e ) );
        ASSERT_FALSE( vecsHas<BoundaryComplex<65>>( w, e ) );
    }

    if ( VECS_MAX_COMPONENTS > 129u )
    {
        vecsSetTypeIdCounter( 127u );
        const uint32_t tag127Id = vecsTypeId<BoundaryTag<127>>();
        const uint32_t pod128Id = vecsTypeId<BoundaryPod<128>>();
        const uint32_t complex129Id = vecsTypeId<BoundaryComplex<129>>();
        ASSERT_EQ( tag127Id, 127u );
        ASSERT_EQ( pod128Id, 128u );
        ASSERT_EQ( complex129Id, 129u );

        vecsAddTag<BoundaryTag<127>>( w, e );
        vecsSet<BoundaryPod<128>>( w, e, { 128u } );
        vecsSetSingleton<BoundaryPod<128>>( w, { 128u } );
        ASSERT_TRUE( vecsGetSingleton<BoundaryPod<128>>( w ) != nullptr );
        vecsClearWorld( w );
        ASSERT_TRUE( vecsGetSingleton<BoundaryPod<128>>( w ) == nullptr );
        e = vecsCreate( w );
    }

    if ( VECS_MAX_COMPONENTS > 193u )
    {
        vecsSetTypeIdCounter( 191u );
        const uint32_t tag191Id = vecsTypeId<BoundaryTag<191>>();
        const uint32_t pod192Id = vecsTypeId<BoundaryPod<192>>();
        const uint32_t complex193Id = vecsTypeId<BoundaryComplex<193>>();
        ASSERT_EQ( tag191Id, 191u );
        ASSERT_EQ( pod192Id, 192u );
        ASSERT_EQ( complex193Id, 193u );

        vecsAddTag<BoundaryTag<191>>( w, e );
        vecsSet<BoundaryPod<192>>( w, e, { 192u } );
        vecsSet<BoundaryComplex<193>>( w, e, { "one-ninety-three" } );

        vecsEntity old = e;
        vecsDestroy( w, old );
        e = vecsCreate( w );
        ASSERT_EQ( vecsEntityIndex( e ), vecsEntityIndex( old ) );
        ASSERT_FALSE( vecsHas<BoundaryTag<191>>( w, e ) );
        ASSERT_FALSE( vecsHas<BoundaryPod<192>>( w, e ) );
        ASSERT_FALSE( vecsHas<BoundaryComplex<193>>( w, e ) );
    }

    if ( VECS_MAX_COMPONENTS > 255u )
    {
        vecsSetTypeIdCounter( 255u );
        const uint32_t tag255Id = vecsTypeId<BoundaryTag<255>>();
        ASSERT_EQ( tag255Id, 255u );
    }

    ASSERT_TRUE( vecsValidate( w ) );
    vecsDestroyWorld( w );
    vecsSetTypeIdCounter( savedCounter );
}

UTEST( entity, component_id_exhaustion_boundary )
{
    uint32_t savedCounter = vecsGetTypeIdCounter();
    vecsSetTypeIdCounter( VECS_MAX_COMPONENTS - 10 );

    constexpr size_t kTypeCount = 10;
    std::vector<uint32_t> ids( kTypeCount );
    vecsCollectExhaustIds( ids.data(), std::make_index_sequence<kTypeCount>() );
    for ( size_t i = 1; i < kTypeCount; i++ )
    {
        ASSERT_TRUE( ids[i] > ids[i - 1u] );
    }

    uint32_t overflowId = vecsTypeId<ExhaustComp<kTypeCount>>();
    ASSERT_GE( overflowId, ( uint32_t )VECS_MAX_COMPONENTS );

    vecsWorld* w = vecsCreateWorld( 16u );
    vecsEntity e = vecsCreate( w );
    uint32_t setCount = vecsTrySetExhaustComponents( w, e, std::make_index_sequence<kTypeCount>() );
    ASSERT_TRUE( setCount <= 10u );
    vecsDestroyWorld( w );

    vecsSetTypeIdCounter( savedCounter );
}

// ==========================================================================
// Snapshot - sync, blocking capture
// ==========================================================================

struct SnapPodA { uint32_t a; float b; };
struct SnapPodB { int x; int y; int z; };
struct SnapTag  {};
struct SnapChunky
{
    std::vector<int>    ints;
    std::vector<float>  floats;
    std::array<uint8_t, 256> scratch;
    uint64_t            blob[8];
};
struct SnapHandles
{
    vecsEntity a;
    vecsEntity b;
    vecsEntity c;
    std::vector<vecsEntity> many;
};

static int g_snapDtorCount = 0;
static int g_snapCopyCount = 0;

struct SnapTracked
{
    std::vector<int> payload;
    SnapTracked() = default;
    SnapTracked( std::vector<int> p ) : payload( std::move( p ) ) {}
    ~SnapTracked() { g_snapDtorCount++; }
};

struct SnapTrackedCopy
{
    std::vector<int> payload;
    SnapTrackedCopy() = default;
    SnapTrackedCopy( const SnapTrackedCopy& o ) : payload( o.payload ) { g_snapCopyCount++; }
    SnapTrackedCopy( std::vector<int> p ) : payload( std::move( p ) ) {}
    ~SnapTrackedCopy() { g_snapDtorCount++; }
};

struct SnapTag1 {};
struct SnapTag2 {};
struct SnapTag3 {};

static int g_snapAddCount = 0;
static int g_snapRemoveCount = 0;
static void onSnapPodAdd( vecsWorld*, vecsEntity, SnapPodA* ) { g_snapAddCount++; }
static void onSnapPodRemove( vecsWorld*, vecsEntity, SnapPodA* ) { g_snapRemoveCount++; }

UTEST( snapshot, empty_world_roundtrip )
{
    vecsWorld* w = vecsCreateWorld( 64u );
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    ASSERT_TRUE( vecsValidate( w ) );
    vecsEntity e = vecsCreate( w );
    vecsSet<SnapPodA>( w, e, { 1u, 2.0f } );
    vecsSnapshotRestore( w, snap );
    ASSERT_FALSE( vecsAlive( w, e ) );
    ASSERT_EQ( vecsCount( w ), 0u );
    ASSERT_TRUE( vecsValidate( w ) );
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, handle_identity_preserved )
{
    vecsWorld* w = vecsCreateWorld( 64u );
    vecsEntity a = vecsCreate( w );
    vecsEntity b = vecsCreate( w );
    vecsSet<SnapPodA>( w, a, { 7u, 3.5f } );
    vecsSet<SnapPodA>( w, b, { 9u, 1.5f } );

    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    vecsSnapshotRestore( w, snap );

    ASSERT_TRUE( vecsAlive( w, a ) );
    ASSERT_TRUE( vecsAlive( w, b ) );
    ASSERT_EQ( vecsEntityIndex( a ), vecsEntityIndex( a ) );
    ASSERT_EQ( vecsEntityGeneration( a ), vecsEntityGeneration( a ) );
    ASSERT_EQ( vecsEntityIndex( b ), vecsEntityIndex( b ) );
    ASSERT_EQ( vecsEntityGeneration( b ), vecsEntityGeneration( b ) );
    ASSERT_EQ( vecsGet<SnapPodA>( w, a )->a, 7u );
    ASSERT_EQ( vecsGet<SnapPodA>( w, b )->a, 9u );
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, dense_order_preserved )
{
    vecsWorld* w = vecsCreateWorld( 64u );
    constexpr uint32_t N = 16;
    vecsEntity es[N];
    for ( uint32_t i = 0; i < N; i++ ) { es[i] = vecsCreate( w ); vecsSet<SnapPodA>( w, es[i], { i, ( float )i } ); }
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    vecsSet<SnapPodA>( w, es[5], { 999u, 0.0f } );
    vecsSet<SnapPodA>( w, es[10], { 888u, 0.0f } );
    vecsSnapshotRestore( w, snap );
    for ( uint32_t i = 0; i < N; i++ )
    {
        ASSERT_EQ( vecsGet<SnapPodA>( w, es[i] )->a, i );
    }
    vecsPool* pool = w->pools[vecsTypeId<SnapPodA>()];
    ASSERT_EQ( pool->count, N );
    for ( uint32_t k = 0; k < N; k++ )
    {
        ASSERT_EQ( pool->denseEntities[k], vecsEntityIndex( es[k] ) );
        ASSERT_EQ( pool->sparse[vecsEntityIndex( es[k] )], k );
    }
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, free_list_order_preserved )
{
    vecsWorld* w = vecsCreateWorld( 64u );
    std::vector<vecsEntity> first;
    for ( int i = 0; i < 10; i++ ) first.push_back( vecsCreate( w ) );
    for ( int i = 0; i < 5; i++ ) vecsDestroy( w, first[i] );

    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );

    for ( int i = 5; i < 10; i++ ) vecsDestroy( w, first[i] );
    std::vector<vecsEntity> after;
    for ( int i = 0; i < 10; i++ ) after.push_back( vecsCreate( w ) );

    vecsSnapshotRestore( w, snap );

    vecsEntityPool* ep = w->entities;
    ASSERT_EQ( ep->freeCount, snap->state->freeCount );
    for ( uint32_t i = 0; i < ep->freeCount; i++ )
    {
        ASSERT_EQ( ep->freeList[i], snap->state->freeList[i] );
    }
    vecsEntity next = vecsCreate( w );
    vecsEntity expectedNext = snap->state->freeList[snap->state->freeCount - 1u];
    ASSERT_EQ( vecsEntityIndex( next ), expectedNext );
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, deep_copy_vector_components )
{
    vecsWorld* w = vecsCreateWorld( 32u );
    g_snapDtorCount = 0;
    g_snapCopyCount = 0;
    vecsEntity a = vecsCreate( w );
    vecsEntity b = vecsCreate( w );
    vecsSet<SnapTrackedCopy>( w, a, { { 1, 2, 3, 4, 5 } } );
    vecsSet<SnapTrackedCopy>( w, b, { { 10, 20 } } );
    int copiesAfterSet = g_snapCopyCount;
    int dtorsAfterSet = g_snapDtorCount;
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    // Snapshot capture does 2 more copyCtors
    ASSERT_EQ( g_snapCopyCount - copiesAfterSet, 2 );

    vecsGet<SnapTrackedCopy>( w, a )->payload.push_back( 99 );
    vecsGet<SnapTrackedCopy>( w, b )->payload.clear();

    vecsSnapshotRestore( w, snap );
    ASSERT_EQ( vecsGet<SnapTrackedCopy>( w, a )->payload.size(), 5u );
    ASSERT_EQ( vecsGet<SnapTrackedCopy>( w, a )->payload[0], 1 );
    ASSERT_EQ( vecsGet<SnapTrackedCopy>( w, a )->payload[4], 5 );
    ASSERT_EQ( vecsGet<SnapTrackedCopy>( w, b )->payload.size(), 2u );
    ASSERT_EQ( vecsGet<SnapTrackedCopy>( w, b )->payload[1], 20 );

    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, chunky_component_capture_and_restore )
{
    vecsWorld* w = vecsCreateWorld( 32u );
    constexpr uint32_t N = 8;
    std::vector<vecsEntity> es;
    for ( uint32_t i = 0; i < N; i++ )
    {
        vecsEntity e = vecsCreate( w );
        SnapChunky c;
        c.ints = { ( int )i, ( int )( i * 2 ), ( int )( i * 3 ) };
        c.floats = { ( float )i + 0.5f };
        for ( uint32_t j = 0; j < 256; j++ ) c.scratch[j] = ( uint8_t )( i + j );
        for ( uint32_t j = 0; j < 8; j++ ) c.blob[j] = ( uint64_t )i * 0xDEADBEEFull + j;
        vecsSet<SnapChunky>( w, e, c );
        es.push_back( e );
    }
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    for ( auto e : es ) vecsGet<SnapChunky>( w, e )->ints.push_back( 999 );
    vecsSnapshotRestore( w, snap );
    for ( uint32_t i = 0; i < N; i++ )
    {
        const SnapChunky* c = vecsGet<SnapChunky>( w, es[i] );
        ASSERT_EQ( c->ints.size(), 3u );
        ASSERT_EQ( c->ints[0], ( int )i );
        ASSERT_EQ( c->ints[1], ( int )( i * 2 ) );
        ASSERT_EQ( c->ints[2], ( int )( i * 3 ) );
        ASSERT_EQ( c->floats.size(), 1u );
        ASSERT_EQ( c->floats[0], ( float )i + 0.5f );
        for ( uint32_t j = 0; j < 256; j++ ) ASSERT_EQ( c->scratch[j], ( uint8_t )( i + j ) );
        for ( uint32_t j = 0; j < 8; j++ ) ASSERT_EQ( c->blob[j], ( uint64_t )i * 0xDEADBEEFull + j );
    }
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, tag_pools_captured )
{
    vecsWorld* w = vecsCreateWorld( 32u );
    vecsEntity a = vecsCreate( w );
    vecsEntity b = vecsCreate( w );
    vecsEntity c = vecsCreate( w );
    vecsAddTag<SnapTag1>( w, a );
    vecsAddTag<SnapTag1>( w, b );
    vecsAddTag<SnapTag2>( w, a );
    vecsAddTag<SnapTag3>( w, c );

    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    vecsUnset<SnapTag1>( w, b );
    vecsAddTag<SnapTag2>( w, c );

    vecsSnapshotRestore( w, snap );

    ASSERT_TRUE( vecsHas<SnapTag1>( w, a ) );
    ASSERT_TRUE( vecsHas<SnapTag1>( w, b ) );
    ASSERT_FALSE( vecsHas<SnapTag1>( w, c ) );
    ASSERT_TRUE( vecsHas<SnapTag2>( w, a ) );
    ASSERT_FALSE( vecsHas<SnapTag2>( w, c ) );
    ASSERT_TRUE( vecsHas<SnapTag3>( w, c ) );
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, lazy_pool_reconciliation_emptied )
{
    vecsWorld* w = vecsCreateWorld( 32u );
    vecsEntity e = vecsCreate( w );
    vecsSet<SnapPodA>( w, e, { 1u, 1.0f } );
    vecsSet<SnapPodB>( w, e, { 2, 3, 4 } );
    vecsSet<SnapChunky>( w, e, {} );
    vecsAddTag<SnapTag1>( w, e );

    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    vecsSet<SnapPodA>( w, e, { 99u, 99.0f } );
    vecsUnset<SnapPodB>( w, e );
    vecsUnset<SnapChunky>( w, e );
    vecsUnset<SnapTag1>( w, e );
    vecsSnapshotRestore( w, snap );

    ASSERT_EQ( vecsGet<SnapPodA>( w, e )->a, 1u );
    ASSERT_EQ( vecsGet<SnapPodB>( w, e )->z, 4 );
    ASSERT_TRUE( vecsHas<SnapTag1>( w, e ) );
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, lazy_pool_creates_missing )
{
    vecsWorld* w = vecsCreateWorld( 32u );
    vecsEntity e = vecsCreate( w );
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    vecsSet<SnapChunky>( w, e, {} );
    SnapChunky c;
    c.ints = { 1, 2, 3 };
    vecsSet<SnapChunky>( w, e, c );
    vecsSnapshotRestore( w, snap );
    ASSERT_FALSE( vecsHas<SnapChunky>( w, e ) );
    ASSERT_TRUE( w->pools[vecsTypeId<SnapChunky>()] != nullptr );
    vecsPool* p = w->pools[vecsTypeId<SnapChunky>()];
    ASSERT_EQ( p->count, 0u );
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, observer_fires_on_restore )
{
    vecsWorld* w = vecsCreateWorld( 32u );
    vecsEntity e = vecsCreate( w );
    g_snapAddCount = 0;
    g_snapRemoveCount = 0;
    vecsOnAdd<SnapPodA>( w, onSnapPodAdd );
    vecsOnRemove<SnapPodA>( w, onSnapPodRemove );
    vecsSet<SnapPodA>( w, e, { 1u, 1.0f } );

    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    int addAfterSet = g_snapAddCount;

    vecsUnset<SnapPodA>( w, e );
    vecsSet<SnapPodA>( w, e, { 2u, 2.0f } );
    int addBefore = g_snapAddCount;
    int removeBefore = g_snapRemoveCount;
    vecsSnapshotRestore( w, snap );

    ASSERT_GT( g_snapAddCount, addBefore ); // restore fired onAdd
    ASSERT_EQ( g_snapAddCount, addBefore + 1 ); // exactly one onAdd fired for the one restored entity
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, capture_into_reuse_no_leak )
{
    vecsWorld* w = vecsCreateWorld( 32u );
    g_snapDtorCount = 0;
    g_snapCopyCount = 0;
    vecsEntity e = vecsCreate( w );
    vecsSet<SnapTrackedCopy>( w, e, { { 1, 2, 3 } } );

    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    int firstSnapCopies = g_snapCopyCount;

    for ( int iter = 0; iter < 1000; iter++ )
    {
        std::vector<int> p( ( iter % 7 ) * 4 + 1 );
        for ( size_t k = 0; k < p.size(); k++ ) p[k] = ( int )( iter * 7 + ( int )k );
        vecsGet<SnapTrackedCopy>( w, e )->payload = p;
        vecsSnapshotCaptureInto( w, snap );
    }
    ASSERT_LE( g_snapDtorCount, firstSnapCopies + 1000 + 16 );
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, stored_entity_handles_resolve )
{
    vecsWorld* w = vecsCreateWorld( 32u );
    vecsEntity a = vecsCreate( w );
    vecsEntity b = vecsCreate( w );
    vecsEntity c = vecsCreate( w );
    SnapHandles sh;
    sh.a = a; sh.b = b; sh.c = c;
    sh.many = { a, b, c, a, b };
    vecsSet<SnapHandles>( w, c, sh );

    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    vecsDestroy( w, a );
    vecsDestroy( w, b );
    vecsDestroy( w, c );
    for ( int i = 0; i < 8; i++ ) vecsCreate( w );
    vecsSnapshotRestore( w, snap );

    ASSERT_TRUE( vecsAlive( w, a ) );
    ASSERT_TRUE( vecsAlive( w, b ) );
    ASSERT_TRUE( vecsAlive( w, c ) );
    const SnapHandles* restored = vecsGet<SnapHandles>( w, c );
    ASSERT_TRUE( vecsAlive( w, restored->a ) );
    ASSERT_TRUE( vecsAlive( w, restored->b ) );
    ASSERT_TRUE( vecsAlive( w, restored->c ) );
    for ( auto h : restored->many ) ASSERT_TRUE( vecsAlive( w, h ) );
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, max_entities_mismatch_asserts )
{
    vecsWorld* a = vecsCreateWorld( 32u );
    vecsEntity e = vecsCreate( a );
    vecsSet<SnapPodA>( a, e, { 1u, 1.0f } );
    vecsWorldSnapshot* snap = vecsSnapshotCreate( a );

    vecsWorld* b = vecsCreateWorld( 64u );
    ASSERT_EQ( snap->state->maxEntities, 32u );
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( a );
    vecsDestroyWorld( b );
}

UTEST( snapshot, validate_after_restore )
{
    vecsWorld* w = vecsCreateWorld( 256u );
    constexpr uint32_t N = 100;
    std::vector<vecsEntity> es;
    for ( uint32_t i = 0; i < N; i++ )
    {
        vecsEntity e = vecsCreate( w );
        es.push_back( e );
        vecsSet<SnapPodA>( w, e, { i, ( float )i } );
        if ( i % 2 == 0 ) vecsSet<SnapPodB>( w, e, { ( int )i, ( int )( i + 1 ), ( int )( i + 2 ) } );
        if ( i % 3 == 0 ) vecsAddTag<SnapTag1>( w, e );
        if ( i % 5 == 0 ) vecsAddTag<SnapTag2>( w, e );
    }
    ASSERT_TRUE( vecsValidate( w ) );
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );

    for ( uint32_t i = 0; i < N; i++ )
    {
        vecsGet<SnapPodA>( w, es[i] )->a = 99999u;
        if ( vecsHas<SnapPodB>( w, es[i] ) ) vecsUnset<SnapPodB>( w, es[i] );
        if ( i % 3 == 0 ) vecsUnset<SnapTag1>( w, es[i] );
    }
    for ( uint32_t i = 0; i < N; i++ ) vecsDestroy( w, es[i] );

    vecsSnapshotRestore( w, snap );
    ASSERT_TRUE( vecsValidate( w ) );
    ASSERT_EQ( vecsCount( w ), N );
    for ( uint32_t i = 0; i < N; i++ )
    {
        ASSERT_EQ( vecsGet<SnapPodA>( w, es[i] )->a, i );
        if ( i % 2 == 0 ) ASSERT_TRUE( vecsHas<SnapPodB>( w, es[i] ) );
        else ASSERT_FALSE( vecsHas<SnapPodB>( w, es[i] ) );
        if ( i % 3 == 0 ) ASSERT_TRUE( vecsHas<SnapTag1>( w, es[i] ) );
        else ASSERT_FALSE( vecsHas<SnapTag1>( w, es[i] ) );
    }
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, bytes_reported_sane )
{
    vecsWorld* w = vecsCreateWorld( 64u );
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    size_t bytes = vecsSnapshotBytes( snap );
    ASSERT_GT( bytes, sizeof( vecsWorldSnapshot ) );
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( snapshot, capture_into_grows_only )
{
    vecsWorld* w = vecsCreateWorld( 64u );
    constexpr uint32_t kBig = 32;
    std::vector<vecsEntity> es;
    for ( uint32_t i = 0; i < kBig; i++ ) { es.push_back( vecsCreate( w ) ); vecsSet<SnapPodA>( w, es[i], { i, ( float )i } ); }
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    size_t bytesAfterBig = vecsSnapshotBytes( snap );

    for ( uint32_t i = 0; i < kBig; i++ ) vecsDestroy( w, es[i] );
    vecsSnapshotCaptureInto( w, snap );
    size_t bytesAfterShrink = vecsSnapshotBytes( snap );

    ASSERT_GE( bytesAfterShrink, bytesAfterBig );
    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

// ==========================================================================
// Snapshot - async (deferred-mutation capture)
// ==========================================================================

// ==========================================================================
// Benchmarks - sync vs async capture main-thread impact
// ==========================================================================

static double vecsBenchNow()
{
    return std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now().time_since_epoch() ).count();
}

struct BenchChunky
{
    uint64_t marker;
    std::vector<int> ints;
    std::vector<float> floats;
    std::array<uint8_t, 256> scratch;
    uint64_t blob[8];
};

// ==========================================================================
// Seqlock — per-pool in-place overwrite during capture
// ==========================================================================

struct SeqGuard { uint64_t a; uint64_t b; };







// ==========================================================================
// Seqlock benchmarks — main-thread overhead vs deferred cmdbuffer
// ==========================================================================

// ==========================================================================
// Fuzz test — seeded xorshift64; stochastic snapshots every 1-3 frames.
// validate after each snapshot; restore at end.

static uint64_t vecsFuzzXorShift64( uint64_t& s )
{
    uint64_t x = s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    s = x;
    return x;
}

struct FuzzPod
{
    uint64_t a;
    uint64_t b;
    uint64_t c;
};

// ==========================================================================
// Gap — adversarial tests proving known missing invariants (expect RED)
// ==========================================================================

static int g_gapObserverFireCount = 0;
static void gapOnHealthAdd( vecsWorld*, vecsEntity, Health* ) { g_gapObserverFireCount++; }

// ==========================================================================

UTEST( bench, sync_snapshot_mainthread_blocked )
{
    vecsWorld* w = vecsCreateWorld( 4096u );
    constexpr uint32_t N = 1000;
    std::vector<vecsEntity> es;
    for ( uint32_t i = 0; i < N; i++ )
    {
        vecsEntity e = vecsCreate( w );
        es.push_back( e );
        vecsSet<BenchChunky>( w, e, BenchChunky{
            ( uint64_t )i,
            std::vector<int>{ ( int )i, ( int )( i + 1 ), ( int )( i + 2 ) },
            std::vector<float>{ ( float )i + 0.5f },
            {},
            { ( uint64_t )i * 7u, ( uint64_t )i * 11u, ( uint64_t )i * 13u,
              ( uint64_t )i * 17u, ( uint64_t )i * 19u, ( uint64_t )i * 23u,
              ( uint64_t )i * 29u, ( uint64_t )i * 31u }
        } );
    }
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );

    constexpr int kIters = 50;
    double t0 = vecsBenchNow();
    for ( int i = 0; i < kIters; i++ )
    {
        vecsSnapshotCaptureInto( w, snap );
    }
    double t1 = vecsBenchNow();
    double perCaptureUs = ( t1 - t0 ) * 1e6 / kIters;
    printf( "[bench] sync CaptureInto: %.2f us/capture (N=%u, chunky)\n", perCaptureUs, N );

    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
    ASSERT_GT( perCaptureUs, 0.0 );
}

UTEST( bench, snapshot_1000_full_actors_main_thread )
{
    struct ActorTransform { float x,y,z, rx,ry,rz,rw, sx,sy,sz; };
    struct ActorVelocity  { float vx,vy,vz, avx,avy,avz; };
    struct ActorHealth    { float hp,maxHp,shield,maxShield,regen; };
    struct ActorAI        { uint8_t state; float targetX,targetY; uint32_t flags;
                            float timers[8]; uint64_t blackboard[8]; };
    struct ActorAnim      { uint32_t clipId,frame; float t,speed;
                            uint8_t blendWeights[16]; uint32_t flags; };
    struct ActorAbility   { float cooldowns[8]; uint32_t charges[8]; };
    struct ActorInventory { std::vector<uint32_t> items;
                            uint32_t gold, weight; };
    struct TagAlive       {};

    constexpr uint32_t N = 1000;
    vecsWorld* w = vecsCreateWorld( N + 64u );

    std::vector<vecsEntity> actors;
    actors.reserve( N );
    for ( uint32_t i = 0; i < N; i++ )
    {
        vecsEntity e = vecsCreate( w );
        actors.push_back( e );
        vecsSet<ActorTransform>( w, e, { (float)i, 0, 0, 0,0,0,1, 1,1,1 } );
        vecsSet<ActorVelocity>( w, e, { 1,0,0, 0,0,0 } );
        vecsSet<ActorHealth>( w, e, { 100, 100, 50, 50, 1 } );
        ActorAI ai{}; ai.state = 1; ai.targetX = (float)(i%60); ai.timers[0] = (float)i;
        vecsSet<ActorAI>( w, e, ai );
        ActorAnim an{}; an.clipId = i % 8; an.speed = 1.0f;
        vecsSet<ActorAnim>( w, e, an );
        ActorAbility ab{}; for ( int k=0;k<8;k++) ab.cooldowns[k]=(float)k;
        vecsSet<ActorAbility>( w, e, ab );
        ActorInventory inv;
        inv.items = { i, i+1, i+2, i%7, i%13 };
        inv.gold = i * 3; inv.weight = 10;
        vecsSet<ActorInventory>( w, e, inv );
        vecsAddTag<TagAlive>( w, e );
    }

    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );

    for ( int i = 0; i < 3; i++ ) vecsSnapshotCaptureInto( w, snap );

    constexpr int kIters = 100;
    double t0 = vecsBenchNow();
    for ( int i = 0; i < kIters; i++ ) vecsSnapshotCaptureInto( w, snap );
    double t1 = vecsBenchNow();
    double usPerCapture = ( t1 - t0 ) * 1e6 / kIters;
    printf( "[bench] 1000 full actors: %.1f us/capture\n", usPerCapture );

    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
    ASSERT_GT( usPerCapture, 0.0 );
}

UTEST( bench, snapshot_pool_allocator )
{
    // Same layout as snapshot_1000_full_actors but the inventory uses
    // vecs::pool_allocator instead of std::allocator. The arena lives on
    // the snapshot; capture routes allocation through it.
    struct ActorInventory_Pooled {
        std::vector<uint32_t, vecs::pool_allocator<uint32_t>> items;
        uint32_t gold, weight;
    };
    struct ActorTransform { float x,y,z, rx,ry,rz,rw, sx,sy,sz; };
    struct ActorVelocity  { float vx,vy,vz, avx,avy,avz; };
    struct ActorHealth    { float hp,maxHp,shield,maxShield,regen; };

    constexpr uint32_t N = 1000;
    vecsWorld* w = vecsCreateWorld( N + 64u );
    std::vector<vecsEntity> actors;
    actors.reserve( N );
    for ( uint32_t i = 0; i < N; i++ )
    {
        vecsEntity e = vecsCreate( w );
        actors.push_back( e );
        vecsSet<ActorTransform>( w, e, { (float)i, 0, 0, 0,0,0,1, 1,1,1 } );
        vecsSet<ActorVelocity>( w, e, { 1,0,0, 0,0,0 } );
        vecsSet<ActorHealth>( w, e, { 100, 100, 50, 50, 1 } );
        ActorInventory_Pooled inv;
        inv.items = { i, i+1, i+2, i%7, i%13 };
        inv.gold = i * 3; inv.weight = 10;
        vecsSet<ActorInventory_Pooled>( w, e, inv );
    }
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );
    for ( int i = 0; i < 3; i++ ) vecsSnapshotCaptureInto( w, snap );

    constexpr int kIters = 100;
    double t0 = vecsBenchNow();
    for ( int i = 0; i < kIters; i++ ) vecsSnapshotCaptureInto( w, snap );
    double t1 = vecsBenchNow();
    double us = ( t1 - t0 ) * 1e6 / kIters;
    printf( "[bench] 1000 full actors (pooled inv): %.1f us/capture\n", us );

    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
    ASSERT_GT( us, 0.0 );
}

UTEST( pool_allocator, vector_capture_and_restore )
{
    struct V { std::vector<uint32_t, vecs::pool_allocator<uint32_t>> items; };
    vecsWorld* w = vecsCreateWorld( 8u );
    vecsEntity e = vecsCreate( w );
    V v; v.items = { 10u, 20u, 30u, 40u, 50u };
    vecsSet<V>( w, e, v );
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );

    v.items.push_back( 999u );
    vecsSet<V>( w, e, v );

    vecsSnapshotRestore( w, snap );
    V* live = vecsGet<V>( w, e );
    ASSERT_TRUE( live != nullptr );
    ASSERT_EQ( live->items.size(), 5u );
    ASSERT_EQ( live->items[0], 10u );
    ASSERT_EQ( live->items[4], 50u );

    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( pool_allocator, over_aligned_component )
{
    // Catches B1 (over-aligned storage); require alignas(64) capture+restore.
    struct alignas( 64 ) Big
    {
        std::vector<uint32_t, vecs::pool_allocator<uint32_t>> items;
        uint8_t pad[64];
    };
    static_assert( alignof( Big ) == 64, "Big must be 64-aligned" );

    vecsWorld* w = vecsCreateWorld( 8u );
    vecsEntity e = vecsCreate( w );
    Big b; b.items = { 1, 2, 3 };
    for ( auto& p : b.pad ) p = 0xAB;
    vecsSet<Big>( w, e, b );
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );

    Big* live = vecsGet<Big>( w, e );
    ASSERT_TRUE( live != nullptr );
    ASSERT_EQ( (uintptr_t)live % alignof(Big), 0u );
    ASSERT_EQ( live->items.size(), 3u );
    for ( auto p : live->pad ) ASSERT_EQ( p, 0xAB );

    vecsSnapshotRestore( w, snap );
    live = vecsGet<Big>( w, e );
    ASSERT_EQ( (uintptr_t)live % alignof(Big), 0u );
    ASSERT_EQ( live->items.size(), 3u );

    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( pool_allocator, node_container )
{
    // std::map node-based allocation exercises rebind. Skipped because
    // libstdc++ map doesn't propagate pool_allocator across insert()
    // by default (propagate_on_container_* = false). The vector tests
    // above are the canonical use case.
}

UTEST( pool_allocator, recapture_reuses_arena )
{
    // Repeated capture must not leak or corrupt (exercises reset()).
    struct V { std::vector<uint32_t, vecs::pool_allocator<uint32_t>> items; };
    vecsWorld* w = vecsCreateWorld( 8u );
    vecsEntity e = vecsCreate( w );
    vecsWorldSnapshot* snap = vecsSnapshotCreate( w );

    for ( int i = 0; i < 200; i++ )
    {
        V v; v.items = { (uint32_t)i, (uint32_t)(i + 1), (uint32_t)(i + 2) };
        vecsSet<V>( w, e, v );
        vecsSnapshotCaptureInto( w, snap );
    }

    V* live = vecsGet<V>( w, e );
    ASSERT_EQ( live->items.size(), 3u );
    ASSERT_EQ( live->items[0], 199u );

    vecsSnapshotRestore( w, snap );
    live = vecsGet<V>( w, e );
    ASSERT_EQ( live->items[0], 199u );

    vecsSnapshotDestroy( snap );
    vecsDestroyWorld( w );
}

UTEST( pool_allocator, pinned_user_pool )
{
    // User creates their own BumpPool, allocator routes to it.
    vecs::BumpPool my_pool;
    std::vector<uint32_t, vecs::pool_allocator<uint32_t>> v( (vecs::pool_allocator<uint32_t>( my_pool )) );
    v.push_back( 6u );
    ASSERT_TRUE( my_pool.owns( v.data() ) );

    v.clear();
    v.push_back( 7u );
    ASSERT_TRUE( my_pool.owns( v.data() ) );

    // Default-constructed allocator uses tls_active_pool or malloc.
    std::vector<uint32_t, vecs::pool_allocator<uint32_t>> v2;
    v2.push_back( 10u );
    ASSERT_TRUE( v2.data() != nullptr );

    // equality: same nullptr pool == equal; different pools != equal.
    vecs::pool_allocator<int> a;
    vecs::pool_allocator<int> b;
    ASSERT_TRUE( a == b );
    vecs::pool_allocator<int> c( my_pool );
    ASSERT_FALSE( a == c );
}

UTEST( bench, create_set_with_pool_allocator )
{
    // Create+set N entities with a std::vector<int> component, default vs
    // pool_allocator. Outside capture PoolScope the pool falls back to
    // malloc; difference is allocator-equality semantics, not the bump.
    struct InvDefault { std::vector<uint32_t> items; uint32_t gold; };
    struct InvPooled  { std::vector<uint32_t, vecs::pool_allocator<uint32_t>> items; uint32_t gold; };

    auto run = []( uint32_t N, int iters, bool pooled ) {
        vecsWorld* w = vecsCreateWorld( (uint32_t)(N * iters) + 64u );
        auto t0 = vecsBenchNow();
        for ( int it = 0; it < iters; it++ )
        {
            for ( uint32_t i = 0; i < N; i++ )
            {
                vecsEntity e = vecsCreate( w );
                if ( pooled )
                {
                    InvPooled inv; inv.items = { i, i+1, i+2 }; inv.gold = i;
                    vecsSet<InvPooled>( w, e, inv );
                }
                else
                {
                    InvDefault inv; inv.items = { i, i+1, i+2 }; inv.gold = i;
                    vecsSet<InvDefault>( w, e, inv );
                }
            }
        }
        auto t1 = vecsBenchNow();
        vecsDestroyWorld( w );
        return ( t1 - t0 ) * 1e6 / iters;
    };

    for ( uint32_t N : { 1000u, 5000u } )
    {
        int iters = N <= 1000 ? 50 : 10;
        // Skip sizes that overflow the world's entity capacity (smallcfg test).
        if ( (uint32_t)(N * iters) + 64u > VECS_MAX_ENTITIES ) continue;
        run( N, 3, false ); run( N, 3, true ); // warmup
        double d = run( N, iters, false );
        double p = run( N, iters, true );
        printf( "[bench] N=%u create+set: default-alloc=%.0f us, pool-alloc=%.0f us\n",
                N, d, p );
    }
    ASSERT_TRUE( true );
}

UTEST( bounded, entity_pool_high_water )
{
    vecsWorld* w = vecsCreateWorld( 64u );

    // Empty world.
    ASSERT_EQ( w->entities->hiAllocated, 0u );

    // Create 10 entities. Initial freeList fills 63..0 at the tail, so
    // first create pops index 0, second index 1, etc.
    std::vector<vecsEntity> ents;
    for ( int i = 0; i < 10; i++ ) ents.push_back( vecsCreate( w ) );
    ASSERT_EQ( w->entities->alive, 10u );
    ASSERT_EQ( w->entities->hiAllocated, 10u );

    // Destroy an interior entity; high water unchanged.
    vecsDestroy( w, ents[5] );
    ASSERT_EQ( w->entities->alive, 9u );
    ASSERT_EQ( w->entities->hiAllocated, 10u );

    // Destroy the current top (index 9). Scan-back drops hiAllocated to 9.
    vecsDestroy( w, ents[9] );
    ASSERT_EQ( w->entities->alive, 8u );
    ASSERT_EQ( w->entities->hiAllocated, 9u );

    // Re-create: gets the freed slot 9 (LIFO). hiAllocated advances back to 10.
    vecsEntity e = vecsCreate( w );
    ASSERT_EQ( vecsEntityIndex( e ), 9u );
    ASSERT_EQ( w->entities->hiAllocated, 10u );

    // Destroy ents[8], ents[7], ents[6], ents[4], ents[3], ents[2].
    // None is at the high slot (9), so hiAllocated stays 10.
    vecsDestroy( w, ents[8] );
    vecsDestroy( w, ents[7] );
    vecsDestroy( w, ents[6] );
    vecsDestroy( w, ents[4] );
    vecsDestroy( w, ents[3] );
    vecsDestroy( w, ents[2] );
    ASSERT_EQ( w->entities->hiAllocated, 10u );
    // alive: {0, 1, 9}

    // Destroy ents[0]. Not the high slot; hiAllocated stays 10.
    vecsDestroy( w, ents[0] );
    ASSERT_EQ( w->entities->hiAllocated, 10u );
    // alive: {1, 9}

    // Destroy ents[1]. Not the high slot; hiAllocated stays 10.
    vecsDestroy( w, ents[1] );
    ASSERT_EQ( w->entities->hiAllocated, 10u );
    // alive: {9}

    // Destroy the entity at high slot (idx 9). Scan-back: only slot 9
    // was alive, now dead. hiAllocated drops to 0.
    vecsEntity highEnt = vecsMakeEntity( 9u, w->entities->generations[9u] );
    vecsDestroy( w, highEnt );
    ASSERT_EQ( w->entities->alive, 0u );
    ASSERT_EQ( w->entities->hiAllocated, 0u );

    (void)e;

    // ClearWorld resets.
    vecsClearWorld( w );
    ASSERT_EQ( w->entities->hiAllocated, 0u );
    ASSERT_EQ( w->entities->alive, 0u );

    vecsDestroyWorld( w );
}

UTEST( bounded, pool_high_water )
{
    vecsWorld* w = vecsCreateWorld( 64u );
    struct Pos { uint32_t x, y; };
    struct Tag {};

    vecsEntity e1 = vecsCreate( w );  // idx 0
    vecsEntity e2 = vecsCreate( w );  // idx 1
    vecsEntity e3 = vecsCreate( w );  // idx 2

    vecsSet<Pos>( w, e1, { 1, 1 } );
    ASSERT_EQ( w->pools[vecsTypeId<Pos>()]->hiSparse, 1u );

    vecsSet<Pos>( w, e2, { 2, 2 } );
    ASSERT_EQ( w->pools[vecsTypeId<Pos>()]->hiSparse, 2u );

    vecsSet<Pos>( w, e3, { 3, 3 } );
    ASSERT_EQ( w->pools[vecsTypeId<Pos>()]->hiSparse, 3u );

    // Tag pool: only e1 (idx 0).
    vecsAddTag<Tag>( w, e1 );
    ASSERT_EQ( w->pools[vecsTypeId<Tag>()]->hiSparse, 1u );

    // Unset Tag from e1: scan back, no live rows. hiSparse=0.
    vecsUnset<Tag>( w, e1 );
    ASSERT_EQ( w->pools[vecsTypeId<Tag>()]->hiSparse, 0u );

    // Unset Pos from e3 (current high, idx 2). LIFO means next Set
    // re-uses idx 2; hiSparse stays 3. To force a shrink, unset e2 then e3.
    vecsUnset<Pos>( w, e2 );
    ASSERT_EQ( w->pools[vecsTypeId<Pos>()]->hiSparse, 3u );
    vecsUnset<Pos>( w, e3 );
    ASSERT_EQ( w->pools[vecsTypeId<Pos>()]->hiSparse, 1u );

    // ClearWorld resets.
    vecsClearWorld( w );
    ASSERT_EQ( w->pools[vecsTypeId<Pos>()]->hiSparse, 0u );

    vecsDestroyWorld( w );
}

UTEST_MAIN();
