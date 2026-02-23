#include "utest.h"
#include "vecs.h"
#include <chrono>
#include <cstdio>
#include <random>
#include <thread>
#include <atomic>
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
struct IsEnemy {};
struct Dead {};
template< size_t I > struct ExhaustComp { int value; };

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

static double vecsBenchOpsPerSecond( const std::chrono::high_resolution_clock::time_point& start, uint64_t ops )
{
    const auto end = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> elapsed = end - start;
    return elapsed.count() > 0.0 ? ( double )ops / elapsed.count() : 0.0;
}

UTEST( vecs, smoke )
{
    ASSERT_NE( VECS_INVALID_ENTITY, 0ULL );
    ASSERT_EQ( ( uint32_t )VECS_MAX_ENTITIES, 65536u );
    ASSERT_EQ( ( uint32_t )VECS_L2_COUNT, 1024u );
    ASSERT_EQ( ( uint32_t )VECS_TOP_COUNT, 16u );
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
    vecsBitfieldSet( &bf, 0u );
    vecsBitfieldSet( &bf, 63u );
    vecsBitfieldSet( &bf, 64u );
    vecsBitfieldSet( &bf, 4095u );
    vecsBitfieldSet( &bf, 4096u );
    vecsBitfieldSet( &bf, 65535u );
    ASSERT_EQ( vecsBitfieldCount( &bf ), 6u );
    ASSERT_TRUE( vecsBitfieldHas( &bf, 0u ) );
    ASSERT_TRUE( vecsBitfieldHas( &bf, 63u ) );
    ASSERT_TRUE( vecsBitfieldHas( &bf, 64u ) );
    ASSERT_TRUE( vecsBitfieldHas( &bf, 4095u ) );
    ASSERT_TRUE( vecsBitfieldHas( &bf, 4096u ) );
    ASSERT_TRUE( vecsBitfieldHas( &bf, 65535u ) );
    ASSERT_FALSE( vecsBitfieldHas( &bf, 1u ) );
    ASSERT_FALSE( vecsBitfieldHas( &bf, 62u ) );
    ASSERT_FALSE( vecsBitfieldHas( &bf, 65u ) );
}

UTEST( bitfield, iteration_order )
{
    vecsBitfield bf = {};
    uint32_t expected[] = { 5u, 100u, 1000u, 50000u };
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
    for ( uint32_t i = 0; i < 1000u; i++ )
    {
        vecsBitfieldSet( &bf, i * 50u );
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

UTEST( cmdbuf, deferred_create )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );

    uint32_t idx0 = vecsCmdCreate( cb );
    uint32_t idx1 = vecsCmdCreate( cb );
    ASSERT_EQ( vecsCount( w ), 0u );

    vecsFlush( cb );
    ASSERT_EQ( vecsCount( w ), 2u );
    vecsEntity e0 = vecsCmdGetCreated( cb, idx0 );
    vecsEntity e1 = vecsCmdGetCreated( cb, idx1 );
    ASSERT_TRUE( vecsAlive( w, e0 ) );
    ASSERT_TRUE( vecsAlive( w, e1 ) );
    ASSERT_NE( e0, e1 );

    vecsDestroyCommandBuffer( cb );
    vecsDestroyWorld( w );
}

UTEST( cmdbuf, deferred_destroy )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    vecsSet<Position>( w, e, { 1, 2 } );

    vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );
    vecsCmdDestroy( cb, e );
    ASSERT_TRUE( vecsAlive( w, e ) );
    vecsFlush( cb );
    ASSERT_FALSE( vecsAlive( w, e ) );

    vecsDestroyCommandBuffer( cb );
    vecsDestroyWorld( w );
}

UTEST( cmdbuf, deferred_set_component )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );

    vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );
    vecsCmdSet<Position>( cb, e, { 3.0f, 4.0f } );
    ASSERT_FALSE( vecsHas<Position>( w, e ) );
    vecsFlush( cb );
    ASSERT_TRUE( vecsHas<Position>( w, e ) );
    ASSERT_EQ( vecsGet<Position>( w, e )->x, 3.0f );

    vecsDestroyCommandBuffer( cb );
    vecsDestroyWorld( w );
}

UTEST( cmdbuf, deferred_unset_component )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    vecsSet<Position>( w, e, { 1, 2 } );

    vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );
    vecsCmdUnset<Position>( cb, e );
    ASSERT_TRUE( vecsHas<Position>( w, e ) );
    vecsFlush( cb );
    ASSERT_FALSE( vecsHas<Position>( w, e ) );

    vecsDestroyCommandBuffer( cb );
    vecsDestroyWorld( w );
}

UTEST( cmdbuf, destroy_during_iteration )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    for ( int i = 0; i < 10; i++ )
    {
        vecsEntity e = vecsCreate( w );
        vecsSet<Health>( w, e, { i * 10 } );
    }

    vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );
    vecsEach<Health>( w, [&]( vecsEntity e, Health& h )
    {
        if ( h.hp < 50 )
        {
            vecsCmdDestroy( cb, e );
        }
    } );
    vecsFlush( cb );
    ASSERT_EQ( vecsCount( w ), 5u );

    vecsDestroyCommandBuffer( cb );
    vecsDestroyWorld( w );
}

UTEST( batch, create )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity entities[100];
    vecsCreateBatch( w, entities, 100u );
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

UTEST( cmdbuf, deferred_set_parent )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity parent = vecsCreate( w );
    vecsEntity child = vecsCreate( w );
    
    vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );
    vecsCmdSetParent( cb, child, parent );
    
    ASSERT_EQ( vecsGetParentEntity( w, child ), VECS_INVALID_ENTITY );
    
    vecsFlush( cb );
    
    ASSERT_EQ( vecsGetParentEntity( w, child ), parent );
    ASSERT_EQ( vecsGetChildEntityCount( w, parent ), 1u );
    
    vecsDestroyCommandBuffer( cb );
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
    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti += 3u )
    {
        uint32_t end = ( ti + 3u < VECS_TOP_COUNT ) ? ti + 3u : VECS_TOP_COUNT;
        vecsQueryEachParallel<Position, Velocity>( w, q, ti, end, [&]( vecsEntity, Position& p, Velocity& v )
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

UTEST( cmdbuf, empty_flush_is_safe )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );
    vecsFlush( cb );
    ASSERT_EQ( vecsCount( w ), 0u );
    vecsDestroyCommandBuffer( cb );
    vecsDestroyWorld( w );
}

UTEST( cmdbuf, set_then_destroy_same_frame )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );

    vecsCmdSet<Position>( cb, e, { 3.0f, 4.0f } );
    vecsCmdDestroy( cb, e );
    vecsFlush( cb );

    ASSERT_FALSE( vecsAlive( w, e ) );
    ASSERT_EQ( vecsCount( w ), 0u );

    vecsDestroyCommandBuffer( cb );
    vecsDestroyWorld( w );
}

UTEST( cmdbuf, multiple_sets_last_wins )
{
    vecsWorld* w = vecsCreateWorld( 1024u );
    vecsEntity e = vecsCreate( w );
    vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );

    vecsCmdSet<Position>( cb, e, { 1.0f, 2.0f } );
    vecsCmdSet<Position>( cb, e, { 9.0f, 7.0f } );
    vecsFlush( cb );

    Position* p = vecsGet<Position>( w, e );
    ASSERT_TRUE( p != nullptr );
    ASSERT_EQ( p->x, 9.0f );
    ASSERT_EQ( p->y, 7.0f );

    vecsDestroyCommandBuffer( cb );
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
    vecsWorld* w = vecsCreateWorld( 20000u );
    std::mt19937 rng( 1337u );
    std::uniform_real_distribution<float> distPos( -500.0f, 500.0f );
    std::uniform_real_distribution<float> distVel( -50.0f, 50.0f );
    std::uniform_int_distribution<int> bit( 0, 99 );

    for ( uint32_t i = 0; i < 10000u; i++ )
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
    simdResults.reserve( 10000u );
    scalarResults.reserve( 10000u );

    g_veDisableSimd = false;
    vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity e, Position& p, Velocity& v )
    {
        uint64_t h = vecsEntityIndex( e );
        h = ( h * 1315423911u ) ^ ( uint64_t )( p.x * 1000.0f ) ^ ( ( uint64_t )( v.vx * 1000.0f ) << 32 );
        simdResults.push_back( h );
    } );

    g_veDisableSimd = true;
    vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity e, Position& p, Velocity& v )
    {
        uint64_t h = vecsEntityIndex( e );
        h = ( h * 1315423911u ) ^ ( uint64_t )( p.x * 1000.0f ) ^ ( ( uint64_t )( v.vx * 1000.0f ) << 32 );
        scalarResults.push_back( h );
    } );
    g_veDisableSimd = false;

    ASSERT_EQ( scalarResults.size(), simdResults.size() );
    for ( size_t i = 0; i < simdResults.size(); i++ )
    {
        ASSERT_EQ( scalarResults[i], simdResults[i] );
    }

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( threading, concurrent_command_buffers )
{
    const uint32_t targetTotal = ( VECS_MAX_ENTITIES < 100000u ) ? VECS_MAX_ENTITIES : 100000u;
    const uint32_t threadCount = 4u;
    const uint32_t basePerThread = targetTotal / threadCount;
    const uint32_t remainder = targetTotal % threadCount;

    vecsWorld* w = vecsCreateWorld( targetTotal );
    vecsEnsurePool<Position>( w );

    std::vector<vecsCommandBuffer*> buffers( threadCount, nullptr );
    std::vector<std::thread> threads;
    threads.reserve( threadCount );

    for ( uint32_t t = 0; t < threadCount; t++ )
    {
        const uint32_t count = basePerThread + ( t < remainder ? 1u : 0u );
        const uint32_t start = t * basePerThread + ( t < remainder ? t : remainder );
        threads.emplace_back( [&, t, count, start]()
        {
            vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );
            for ( uint32_t i = 0; i < count; i++ )
            {
                uint32_t created = vecsCmdCreate( cb );
                vecsCmdSetCreated<Position>( cb, created, { ( float )( start + i ), ( float )t } );
            }
            buffers[t] = cb;
        } );
    }

    for ( auto& th : threads )
    {
        th.join();
    }

    uint32_t alive = 0u;
    for ( uint32_t t = 0; t < threadCount; t++ )
    {
        vecsCommandBuffer* cb = buffers[t];
        vecsFlush( cb );
        for ( uint32_t i = 0; i < cb->createdCount; i++ )
        {
            vecsEntity e = vecsCmdGetCreated( cb, i );
            ASSERT_NE( e, VECS_INVALID_ENTITY );
            ASSERT_TRUE( vecsAlive( w, e ) );
            Position* p = vecsGet<Position>( w, e );
            ASSERT_TRUE( p != nullptr );
            alive++;
        }
        vecsDestroyCommandBuffer( cb );
    }

    ASSERT_EQ( alive, targetTotal );
    ASSERT_EQ( vecsCount( w ), targetTotal );
    vecsDestroyWorld( w );
}

UTEST( threading, parallel_chunked_iteration_manual )
{
    const uint32_t entityCount = ( VECS_MAX_ENTITIES < 100000u ) ? VECS_MAX_ENTITIES : 100000u;
    vecsWorld* w = vecsCreateWorld( entityCount );
    std::vector<vecsEntity> entities;
    entities.reserve( entityCount );
    for ( uint32_t i = 0; i < entityCount; i++ )
    {
        vecsEntity e = vecsCreate( w );
        entities.push_back( e );
        vecsSet<Position>( w, e, { ( float )i, 0.0f } );
        vecsSet<Velocity>( w, e, { 1.0f, 0.0f } );
    }

    vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
    vecsPool* positionPool = w->pools[vecsTypeId<Position>()];
    vecsPool* velocityPool = w->pools[vecsTypeId<Velocity>()];
    std::atomic<uint32_t> processed = 0u;

    const uint32_t threadCount = 4u;
    const uint32_t chunk = ( VECS_TOP_COUNT + threadCount - 1u ) / threadCount;
    std::vector<std::thread> threads;
    threads.reserve( threadCount );
    for ( uint32_t t = 0; t < threadCount; t++ )
    {
        const uint32_t startTi = t * chunk;
        const uint32_t endTi = ( startTi + chunk < VECS_TOP_COUNT ) ? startTi + chunk : VECS_TOP_COUNT;
        threads.emplace_back( [=, &processed]()
        {
            for ( uint32_t ti = startTi; ti < endTi; ti++ )
            {
                uint64_t top = q->withMask.topMasks[ti];
                while ( top )
                {
                    uint32_t tb = vecsTzcnt( top );
                    uint32_t l2Idx = ti * 64u + tb;
                    uint64_t l2 = q->withMask.l2Masks[l2Idx];
                    while ( l2 )
                    {
                        uint32_t lb = vecsTzcnt( l2 );
                        uint32_t entityIdx = l2Idx * 64u + lb;

                        uint32_t densePos = positionPool->sparse[entityIdx];
                        uint32_t denseVel = velocityPool->sparse[entityIdx];
                        Position* p = ( Position* )( positionPool->denseData + ( size_t )densePos * positionPool->stride );
                        Velocity* v = ( Velocity* )( velocityPool->denseData + ( size_t )denseVel * velocityPool->stride );
                        p->x += 1.0f;
                        v->vx += 1.0f;
                        processed.fetch_add( 1u, std::memory_order_relaxed );

                        l2 &= l2 - 1;
                    }
                    top &= top - 1;
                }
            }
        } );
    }
    for ( auto& th : threads )
    {
        th.join();
    }

    ASSERT_EQ( processed.load( std::memory_order_relaxed ), entityCount );
    for ( uint32_t i = 0; i < entityCount; i++ )
    {
        Position* p = vecsGet<Position>( w, entities[i] );
        Velocity* v = vecsGet<Velocity>( w, entities[i] );
        ASSERT_EQ( p->x, ( float )i + 1.0f );
        ASSERT_EQ( v->vx, 2.0f );
    }

    vecsDestroyQuery( q );
    vecsDestroyWorld( w );
}

UTEST( benchmark, vecs_ops_per_second )
{
    const uint32_t activeCapacity = VECS_MAX_ENTITIES;
    const uint64_t opTarget = 1000000ULL;

    {
        vecsWorld* w = vecsCreateWorld( activeCapacity );
        std::vector<vecsEntity> ring( activeCapacity, VECS_INVALID_ENTITY );
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint64_t i = 0; i < opTarget; i++ )
        {
            uint32_t slot = ( uint32_t )( i % activeCapacity );
            if ( ring[slot] != VECS_INVALID_ENTITY )
            {
                vecsDestroy( w, ring[slot] );
            }
            ring[slot] = vecsCreate( w );
            ASSERT_NE( ring[slot], VECS_INVALID_ENTITY );
        }
        const double ops = vecsBenchOpsPerSecond( start, opTarget );
        std::printf( "[BENCHMARK] Entity Create: %.0f ops/s\n", ops );
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
        std::printf( "[BENCHMARK] Component Insert (Position+Velocity): %.0f ops/s\n", ops );
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
            vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );
            for ( uint32_t i = 0; i < batch; i++ )
            {
                vecsCmdCreate( cb );
            }
            vecsFlush( cb );
            for ( uint32_t i = 0; i < batch; i++ )
            {
                vecsEntity e = vecsCmdGetCreated( cb, i );
                if ( e != VECS_INVALID_ENTITY )
                {
                    vecsDestroy( w, e );
                }
            }
            vecsDestroyCommandBuffer( cb );
            created += batch;
        }
        const double ops = vecsBenchOpsPerSecond( start, createTarget );
        std::printf( "[BENCHMARK] Command Buffer Flush (Create): %.0f ops/s\n", ops );
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

        const uint64_t processedTarget = 1000000ULL;
        uint64_t processedEach = 0u;
        const auto eachStart = std::chrono::high_resolution_clock::now();
        while ( processedEach < processedTarget )
        {
            vecsEach<Position, Velocity>( w, [&]( vecsEntity, Position&, Velocity& )
            {
                processedEach++;
            } );
        }
        const double eachOps = vecsBenchOpsPerSecond( eachStart, processedEach );
        std::printf( "[BENCHMARK] Query Iterate vecsEach (SIMD path): %.0f ops/s\n", eachOps );

        vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
        uint64_t processedCached = 0u;
        const auto queryStart = std::chrono::high_resolution_clock::now();
        while ( processedCached < processedTarget )
        {
            vecsQueryEach<Position, Velocity>( w, q, [&]( vecsEntity, Position&, Velocity& )
            {
                processedCached++;
            } );
        }
        const double queryOps = vecsBenchOpsPerSecond( queryStart, processedCached );
        std::printf( "[BENCHMARK] Query Iterate vecsQueryEach (cached): %.0f ops/s\n", queryOps );
        vecsDestroyQuery( q );
        vecsDestroyWorld( w );
    }
}

UTEST( benchmark, vecs_vs_entt )
{
#if defined( VECS_HAS_ENTT )
    const uint32_t activeCapacity = VECS_MAX_ENTITIES;
    const uint64_t opTarget = 1000000ULL;

    double vecsCreateOps = 0.0;
    double enttCreateOps = 0.0;
    {
        vecsWorld* w = vecsCreateWorld( activeCapacity );
        std::vector<vecsEntity> ring( activeCapacity, VECS_INVALID_ENTITY );
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint64_t i = 0; i < opTarget; i++ )
        {
            uint32_t slot = ( uint32_t )( i % activeCapacity );
            if ( ring[slot] != VECS_INVALID_ENTITY )
            {
                vecsDestroy( w, ring[slot] );
            }
            ring[slot] = vecsCreate( w );
            ASSERT_NE( ring[slot], VECS_INVALID_ENTITY );
        }
        vecsCreateOps = vecsBenchOpsPerSecond( start, opTarget );
        vecsDestroyWorld( w );
    }
    {
        entt::registry registry;
        std::vector<entt::entity> entities;
        entities.resize( ( size_t )opTarget );
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint64_t i = 0; i < opTarget; i++ )
        {
            entities[( size_t )i] = registry.create();
        }
        enttCreateOps = vecsBenchOpsPerSecond( start, opTarget );
    }
    std::printf( "[BENCHMARK] Entity Create | Vecs: %.0f ops/s | EnTT: %.0f ops/s\n", vecsCreateOps, enttCreateOps );

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
        entities.resize( ( size_t )opTarget );
        for ( uint64_t i = 0; i < opTarget; i++ )
        {
            entities[( size_t )i] = registry.create();
        }
        const auto start = std::chrono::high_resolution_clock::now();
        for ( uint64_t i = 0; i < opTarget; i++ )
        {
            const Position p = { ( float )i, 0.0f };
            const Velocity v = { 0.0f, ( float )i };
            registry.emplace_or_replace<Position>( entities[( size_t )i], p );
            registry.emplace_or_replace<Velocity>( entities[( size_t )i], v );
        }
        enttSetOps = vecsBenchOpsPerSecond( start, opTarget * 2u );
    }
    std::printf( "[BENCHMARK] Component Insert | Vecs: %.0f ops/s | EnTT: %.0f ops/s\n", vecsSetOps, enttSetOps );

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
        entities.resize( ( size_t )opTarget );
        for ( uint64_t i = 0; i < opTarget; i++ )
        {
            entt::entity e = registry.create();
            entities[( size_t )i] = e;
            registry.emplace<Position>( e, Position{ ( float )i, 1.0f } );
            registry.emplace<Velocity>( e, Velocity{ 2.0f, ( float )i } );
        }

        uint64_t processed = 0u;
        const auto start = std::chrono::high_resolution_clock::now();
        registry.view<Position, Velocity>().each( [&]( Position&, Velocity& )
        {
            processed++;
        } );
        enttIterOps = vecsBenchOpsPerSecond( start, processed );
    }
    std::printf( "[BENCHMARK] Query Iterate | Vecs: %.0f ops/s | EnTT: %.0f ops/s\n", vecsIterOps, enttIterOps );
#else
    std::printf( "[BENCHMARK] Vecs vs EnTT skipped (entt/entt.hpp not found).\n" );
#endif
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

UTEST_MAIN();
