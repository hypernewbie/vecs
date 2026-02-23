#include "utest.h"
#include "vecs.h"

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

UTEST_MAIN();
