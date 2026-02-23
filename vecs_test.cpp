#include "utest.h"
#include "vecs.h"

struct Position { float x, y; };
struct Velocity { float vx, vy; };
struct Health { int hp; };

UTEST( vecs, smoke )
{
    ASSERT_NE( VECS_INVALID_ENTITY, 0ULL );
    ASSERT_EQ( ( uint32_t )VECS_MAX_ENTITIES, 65536u );
    ASSERT_EQ( ( uint32_t )VECS_L2_COUNT, 1024u );
    ASSERT_EQ( ( uint32_t )VECS_TOP_COUNT, 16u );
}

UTEST( bits, tzcnt_single_bits )
{
    ASSERT_EQ( veTzcnt( 1ULL ), 0u );
    ASSERT_EQ( veTzcnt( 1ULL << 63 ), 63u );
    ASSERT_EQ( veTzcnt( 0x8000ULL ), 15u );
}

UTEST( bits, popcnt )
{
    ASSERT_EQ( vePopcnt( 0ULL ), 0u );
    ASSERT_EQ( vePopcnt( UINT64_MAX ), 64u );
    ASSERT_EQ( vePopcnt( 0xAAAAAAAAAAAAAAAAULL ), 32u );
}

UTEST( entity, pack_unpack )
{
    veEntity e = veMakeEntity( 42u, 7u );
    ASSERT_EQ( veEntityIndex( e ), 42u );
    ASSERT_EQ( veEntityGeneration( e ), 7u );
}

UTEST( entity, create_destroy_cycle )
{
    veEntityPool* pool = veCreateEntityPool( 128u );
    veEntity e1 = veEntityPoolCreate( pool );
    veEntity e2 = veEntityPoolCreate( pool );
    ASSERT_NE( veEntityIndex( e1 ), veEntityIndex( e2 ) );
    ASSERT_TRUE( veEntityPoolAlive( pool, e1 ) );
    ASSERT_TRUE( veEntityPoolAlive( pool, e2 ) );

    veEntityPoolDestroy( pool, e1 );
    ASSERT_FALSE( veEntityPoolAlive( pool, e1 ) );
    ASSERT_TRUE( veEntityPoolAlive( pool, e2 ) );

    veEntity e3 = veEntityPoolCreate( pool );
    ASSERT_EQ( veEntityIndex( e3 ), veEntityIndex( e1 ) );
    ASSERT_EQ( veEntityGeneration( e3 ), veEntityGeneration( e1 ) + 1u );

    veDestroyEntityPool( pool );
}

UTEST( entity, pool_full )
{
    veEntityPool* pool = veCreateEntityPool( 2u );
    veEntityPoolCreate( pool );
    veEntityPoolCreate( pool );
    veEntity e = veEntityPoolCreate( pool );
    ASSERT_EQ( e, VECS_INVALID_ENTITY );
    veDestroyEntityPool( pool );
}

UTEST( entity, generation_overflow_wraps )
{
    veEntityPool* pool = veCreateEntityPool( 4u );
    for ( int i = 0; i < 10; i++ )
    {
        veEntity e = veEntityPoolCreate( pool );
        veEntityPoolDestroy( pool, e );
    }
    veEntity e = veEntityPoolCreate( pool );
    ASSERT_EQ( veEntityGeneration( e ), 10u );
    veEntityPoolDestroy( pool, e );
    veDestroyEntityPool( pool );
}

UTEST( bitfield, set_has_clear )
{
    veBitfield bf = {};
    ASSERT_FALSE( veBitfieldHas( &bf, 0u ) );
    veBitfieldSet( &bf, 0u );
    ASSERT_TRUE( veBitfieldHas( &bf, 0u ) );
    veBitfieldUnset( &bf, 0u );
    ASSERT_FALSE( veBitfieldHas( &bf, 0u ) );
}

UTEST( bitfield, scattered_bits )
{
    veBitfield bf = {};
    veBitfieldSet( &bf, 0u );
    veBitfieldSet( &bf, 63u );
    veBitfieldSet( &bf, 64u );
    veBitfieldSet( &bf, 4095u );
    veBitfieldSet( &bf, 4096u );
    veBitfieldSet( &bf, 65535u );
    ASSERT_EQ( veBitfieldCount( &bf ), 6u );
    ASSERT_TRUE( veBitfieldHas( &bf, 0u ) );
    ASSERT_TRUE( veBitfieldHas( &bf, 63u ) );
    ASSERT_TRUE( veBitfieldHas( &bf, 64u ) );
    ASSERT_TRUE( veBitfieldHas( &bf, 4095u ) );
    ASSERT_TRUE( veBitfieldHas( &bf, 4096u ) );
    ASSERT_TRUE( veBitfieldHas( &bf, 65535u ) );
    ASSERT_FALSE( veBitfieldHas( &bf, 1u ) );
    ASSERT_FALSE( veBitfieldHas( &bf, 62u ) );
    ASSERT_FALSE( veBitfieldHas( &bf, 65u ) );
}

UTEST( bitfield, iteration_order )
{
    veBitfield bf = {};
    uint32_t expected[] = { 5u, 100u, 1000u, 50000u };
    for ( uint32_t idx : expected )
    {
        veBitfieldSet( &bf, idx );
    }

    uint32_t collected[4] = {};
    uint32_t n = 0;
    veBitfieldEach( &bf, [&]( uint32_t index ) { collected[n++] = index; } );

    ASSERT_EQ( n, 4u );
    for ( uint32_t i = 0; i < 4u; i++ )
    {
        ASSERT_EQ( collected[i], expected[i] );
    }
}

UTEST( bitfield, join )
{
    veBitfield a = {}, b = {};
    veBitfieldSet( &a, 10u );
    veBitfieldSet( &a, 20u );
    veBitfieldSet( &a, 30u );
    veBitfieldSet( &b, 20u );
    veBitfieldSet( &b, 30u );
    veBitfieldSet( &b, 40u );

    uint32_t collected[4] = {};
    uint32_t n = 0;
    veBitfieldJoin( &a, &b, [&]( uint32_t index ) { collected[n++] = index; } );

    ASSERT_EQ( n, 2u );
    ASSERT_EQ( collected[0], 20u );
    ASSERT_EQ( collected[1], 30u );
}

UTEST( bitfield, top_mask_auto_clear )
{
    veBitfield bf = {};
    veBitfieldSet( &bf, 100u );
    ASSERT_NE( bf.topMasks[100u / 4096u], 0ULL );
    veBitfieldUnset( &bf, 100u );
    uint32_t n = 0;
    veBitfieldEach( &bf, [&]( uint32_t ) { n++; } );
    ASSERT_EQ( n, 0u );
}

UTEST( bitfield, clear_all )
{
    veBitfield bf = {};
    for ( uint32_t i = 0; i < 1000u; i++ )
    {
        veBitfieldSet( &bf, i * 50u );
    }
    veBitfieldClearAll( &bf );
    ASSERT_EQ( veBitfieldCount( &bf ), 0u );
}

UTEST( bitfield, boundary_last_entity )
{
    veBitfield bf = {};
    veBitfieldSet( &bf, VECS_MAX_ENTITIES - 1u );
    ASSERT_TRUE( veBitfieldHas( &bf, VECS_MAX_ENTITIES - 1u ) );
    ASSERT_EQ( veBitfieldCount( &bf ), 1u );
}

UTEST( pool, add_get_remove )
{
    vePool* pool = veCreatePool( 1024u, sizeof( Position ) );
    Position p = { 1.0f, 2.0f };
    vePoolSet( pool, 0u, &p );
    ASSERT_TRUE( vePoolHas( pool, 0u ) );
    Position* got = ( Position* )vePoolGet( pool, 0u );
    ASSERT_EQ( got->x, 1.0f );
    ASSERT_EQ( got->y, 2.0f );
    vePoolUnset( pool, 0u );
    ASSERT_FALSE( vePoolHas( pool, 0u ) );
    veDestroyPool( pool );
}

UTEST( pool, swap_and_pop_integrity )
{
    vePool* pool = veCreatePool( 1024u, sizeof( int ) );
    int vals[] = { 10, 20, 30 };
    vePoolSet( pool, 5u, &vals[0] );
    vePoolSet( pool, 10u, &vals[1] );
    vePoolSet( pool, 15u, &vals[2] );
    vePoolUnset( pool, 10u );
    ASSERT_FALSE( vePoolHas( pool, 10u ) );
    ASSERT_TRUE( vePoolHas( pool, 5u ) );
    ASSERT_TRUE( vePoolHas( pool, 15u ) );
    ASSERT_EQ( *( int* )vePoolGet( pool, 5u ), 10 );
    ASSERT_EQ( *( int* )vePoolGet( pool, 15u ), 30 );
    veDestroyPool( pool );
}

UTEST( pool, grow )
{
    vePool* pool = veCreatePool( 1024u, sizeof( int ) );
    for ( int i = 0; i < 200; i++ )
    {
        vePoolSet( pool, ( uint32_t )i, &i );
    }
    ASSERT_EQ( pool->count, 200u );
    for ( int i = 0; i < 200; i++ )
    {
        ASSERT_EQ( *( int* )vePoolGet( pool, ( uint32_t )i ), i );
    }
    veDestroyPool( pool );
}

UTEST( world, create_destroy_entity )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity e = veCreate( w );
    ASSERT_TRUE( veAlive( w, e ) );
    veDestroy( w, e );
    ASSERT_FALSE( veAlive( w, e ) );
    veDestroyWorld( w );
}

UTEST( world, component_crud )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity e = veCreate( w );

    veSet<Position>( w, e, { 1.0f, 2.0f } );
    ASSERT_TRUE( veHas<Position>( w, e ) );
    Position* p = veGet<Position>( w, e );
    ASSERT_EQ( p->x, 1.0f );
    ASSERT_EQ( p->y, 2.0f );

    veSet<Position>( w, e, { 3.0f, 4.0f } );
    p = veGet<Position>( w, e );
    ASSERT_EQ( p->x, 3.0f );

    veUnset<Position>( w, e );
    ASSERT_FALSE( veHas<Position>( w, e ) );

    veDestroyWorld( w );
}

UTEST( world, destroy_entity_removes_components )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity e = veCreate( w );
    veSet<Position>( w, e, { 1.0f, 2.0f } );
    veSet<Velocity>( w, e, { 0.1f, 0.2f } );
    veDestroy( w, e );
    veEntity e2 = veCreate( w );
    ASSERT_FALSE( veHas<Position>( w, e2 ) );
    ASSERT_FALSE( veHas<Velocity>( w, e2 ) );
    veDestroyWorld( w );
}

UTEST( world, multiple_entities_multiple_components )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity e1 = veCreate( w );
    veEntity e2 = veCreate( w );
    veEntity e3 = veCreate( w );

    veSet<Position>( w, e1, { 1, 1 } );
    veSet<Position>( w, e2, { 2, 2 } );
    veSet<Velocity>( w, e1, { 10, 10 } );
    veSet<Health>( w, e3, { 100 } );

    ASSERT_TRUE( veHas<Position>( w, e1 ) );
    ASSERT_TRUE( veHas<Position>( w, e2 ) );
    ASSERT_FALSE( veHas<Position>( w, e3 ) );
    ASSERT_TRUE( veHas<Velocity>( w, e1 ) );
    ASSERT_FALSE( veHas<Velocity>( w, e2 ) );
    ASSERT_TRUE( veHas<Health>( w, e3 ) );
    ASSERT_EQ( veCount( w ), 3u );

    veDestroyWorld( w );
}

UTEST( query, each_single_component )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity e1 = veCreate( w );
    veEntity e2 = veCreate( w );
    veEntity e3 = veCreate( w );
    veSet<Position>( w, e1, { 1, 0 } );
    veSet<Position>( w, e2, { 2, 0 } );
    veSet<Position>( w, e3, { 3, 0 } );

    float sum = 0.0f;
    veEach<Position>( w, [&]( veEntity, Position& p ) { sum += p.x; } );
    ASSERT_EQ( sum, 6.0f );

    veDestroyWorld( w );
}

UTEST( query, each_two_components )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity e1 = veCreate( w );
    veEntity e2 = veCreate( w );
    veEntity e3 = veCreate( w );

    veSet<Position>( w, e1, { 1, 0 } );
    veSet<Position>( w, e2, { 2, 0 } );
    veSet<Position>( w, e3, { 3, 0 } );
    veSet<Velocity>( w, e1, { 10, 0 } );
    veSet<Velocity>( w, e3, { 30, 0 } );

    float sum = 0.0f;
    veEach<Position, Velocity>( w, [&]( veEntity, Position& p, Velocity& v ) { sum += p.x + v.vx; } );
    ASSERT_EQ( sum, 44.0f );

    veDestroyWorld( w );
}

UTEST( query, each_three_components )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity e1 = veCreate( w );
    veEntity e2 = veCreate( w );

    veSet<Position>( w, e1, { 1, 0 } );
    veSet<Velocity>( w, e1, { 10, 0 } );
    veSet<Health>( w, e1, { 100 } );
    veSet<Position>( w, e2, { 2, 0 } );
    veSet<Velocity>( w, e2, { 20, 0 } );

    int count = 0;
    veEach<Position, Velocity, Health>( w, [&]( veEntity, Position&, Velocity&, Health& h )
    {
        ASSERT_EQ( h.hp, 100 );
        count++;
    } );
    ASSERT_EQ( count, 1 );

    veDestroyWorld( w );
}

UTEST( query, each_empty_world )
{
    veWorld* w = veCreateWorld( 1024u );
    int count = 0;
    veEach<Position>( w, [&]( veEntity, Position& ) { count++; } );
    ASSERT_EQ( count, 0 );
    veDestroyWorld( w );
}

UTEST( query, each_no_matches )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity e = veCreate( w );
    veSet<Position>( w, e, { 1, 1 } );

    int count = 0;
    veEach<Position, Velocity>( w, [&]( veEntity, Position&, Velocity& ) { count++; } );
    ASSERT_EQ( count, 0 );

    veDestroyWorld( w );
}

UTEST( query, each_iteration_order )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity entities[5];
    for ( int i = 0; i < 5; i++ )
    {
        entities[i] = veCreate( w );
        veSet<Position>( w, entities[i], { ( float )i, 0 } );
    }

    float prev = -1.0f;
    veEach<Position>( w, [&]( veEntity, Position& p )
    {
        ASSERT_GT( p.x, prev );
        prev = p.x;
    } );

    veDestroyWorld( w );
}

UTEST( singleton, set_get )
{
    struct Gravity { float g; };
    veWorld* w = veCreateWorld( 1024u );
    veSetSingleton<Gravity>( w, { 9.81f } );
    Gravity* g = veGetSingleton<Gravity>( w );
    ASSERT_TRUE( g != nullptr );
    ASSERT_EQ( g->g, 9.81f );
    veDestroyWorld( w );
}

UTEST( singleton, get_unset_returns_null )
{
    struct Wind { float speed; };
    veWorld* w = veCreateWorld( 1024u );
    Wind* wind = veGetSingleton<Wind>( w );
    ASSERT_TRUE( wind == nullptr );
    veDestroyWorld( w );
}

UTEST( query, mutate_during_iteration )
{
    veWorld* w = veCreateWorld( 1024u );
    for ( int i = 0; i < 10; i++ )
    {
        veEntity e = veCreate( w );
        veSet<Position>( w, e, { ( float )i, 0 } );
    }

    veEach<Position>( w, [&]( veEntity, Position& p ) { p.x *= 2.0f; } );

    float sum = 0.0f;
    veEach<Position>( w, [&]( veEntity, Position& p ) { sum += p.x; } );
    ASSERT_EQ( sum, 90.0f );

    veDestroyWorld( w );
}

UTEST( clone, basic )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity src = veCreate( w );
    veSet<Position>( w, src, { 5.0f, 10.0f } );
    veSet<Velocity>( w, src, { 1.0f, 2.0f } );

    veEntity dst = veClone( w, src );
    ASSERT_NE( dst, src );
    ASSERT_TRUE( veAlive( w, dst ) );
    ASSERT_TRUE( veHas<Position>( w, dst ) );
    ASSERT_TRUE( veHas<Velocity>( w, dst ) );

    Position* p = veGet<Position>( w, dst );
    ASSERT_EQ( p->x, 5.0f );
    ASSERT_EQ( p->y, 10.0f );
    p->x = 99.0f;
    ASSERT_EQ( veGet<Position>( w, src )->x, 5.0f );

    veDestroyWorld( w );
}

UTEST( clone, no_components )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity src = veCreate( w );
    veEntity dst = veClone( w, src );
    ASSERT_TRUE( veAlive( w, dst ) );
    ASSERT_FALSE( veHas<Position>( w, dst ) );
    veDestroyWorld( w );
}

UTEST( cmdbuf, deferred_create )
{
    veWorld* w = veCreateWorld( 1024u );
    veCommandBuffer* cb = veCreateCommandBuffer( w );

    uint32_t idx0 = veCmdCreate( cb );
    uint32_t idx1 = veCmdCreate( cb );
    ASSERT_EQ( veCount( w ), 0u );

    veFlush( cb );
    ASSERT_EQ( veCount( w ), 2u );
    veEntity e0 = veCmdGetCreated( cb, idx0 );
    veEntity e1 = veCmdGetCreated( cb, idx1 );
    ASSERT_TRUE( veAlive( w, e0 ) );
    ASSERT_TRUE( veAlive( w, e1 ) );
    ASSERT_NE( e0, e1 );

    veDestroyCommandBuffer( cb );
    veDestroyWorld( w );
}

UTEST( cmdbuf, deferred_destroy )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity e = veCreate( w );
    veSet<Position>( w, e, { 1, 2 } );

    veCommandBuffer* cb = veCreateCommandBuffer( w );
    veCmdDestroy( cb, e );
    ASSERT_TRUE( veAlive( w, e ) );
    veFlush( cb );
    ASSERT_FALSE( veAlive( w, e ) );

    veDestroyCommandBuffer( cb );
    veDestroyWorld( w );
}

UTEST( cmdbuf, deferred_set_component )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity e = veCreate( w );

    veCommandBuffer* cb = veCreateCommandBuffer( w );
    veCmdSet<Position>( cb, e, { 3.0f, 4.0f } );
    ASSERT_FALSE( veHas<Position>( w, e ) );
    veFlush( cb );
    ASSERT_TRUE( veHas<Position>( w, e ) );
    ASSERT_EQ( veGet<Position>( w, e )->x, 3.0f );

    veDestroyCommandBuffer( cb );
    veDestroyWorld( w );
}

UTEST( cmdbuf, deferred_unset_component )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity e = veCreate( w );
    veSet<Position>( w, e, { 1, 2 } );

    veCommandBuffer* cb = veCreateCommandBuffer( w );
    veCmdUnset<Position>( cb, e );
    ASSERT_TRUE( veHas<Position>( w, e ) );
    veFlush( cb );
    ASSERT_FALSE( veHas<Position>( w, e ) );

    veDestroyCommandBuffer( cb );
    veDestroyWorld( w );
}

UTEST( cmdbuf, destroy_during_iteration )
{
    veWorld* w = veCreateWorld( 1024u );
    for ( int i = 0; i < 10; i++ )
    {
        veEntity e = veCreate( w );
        veSet<Health>( w, e, { i * 10 } );
    }

    veCommandBuffer* cb = veCreateCommandBuffer( w );
    veEach<Health>( w, [&]( veEntity e, Health& h )
    {
        if ( h.hp < 50 )
        {
            veCmdDestroy( cb, e );
        }
    } );
    veFlush( cb );
    ASSERT_EQ( veCount( w ), 5u );

    veDestroyCommandBuffer( cb );
    veDestroyWorld( w );
}

UTEST( batch, create )
{
    veWorld* w = veCreateWorld( 1024u );
    veEntity entities[100];
    veCreateBatch( w, entities, 100u );
    ASSERT_EQ( veCount( w ), 100u );
    for ( int i = 0; i < 100; i++ )
    {
        ASSERT_TRUE( veAlive( w, entities[i] ) );
    }
    veDestroyWorld( w );
}

UTEST_MAIN();
