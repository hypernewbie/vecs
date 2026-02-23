#include "utest.h"
#include "vecs.h"

UTEST( vecs, smoke )
{
    ASSERT_NE( VECS_INVALID_ENTITY, 0ULL );
    ASSERT_EQ( VECS_MAX_ENTITIES, 65536u );
    ASSERT_EQ( VECS_L2_COUNT, 1024u );
    ASSERT_EQ( VECS_TOP_COUNT, 16u );
}

UTEST_MAIN();
