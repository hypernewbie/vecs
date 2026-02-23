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

UTEST_MAIN();
