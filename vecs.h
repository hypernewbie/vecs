/*
    -- Vecs --

    Copyright 2026 UAA Software

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
    associated documentation files (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge, publish, distribute,
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial
    portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
    NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
    OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <cstring>
#include <cassert>
#include <new>
#include <cstdlib>

#ifndef VECS_MAX_ENTITIES
#define VECS_MAX_ENTITIES 65536
#endif

#ifndef VECS_MAX_COMPONENTS
#define VECS_MAX_COMPONENTS 256
#endif

#define VECS_L2_COUNT  ( VECS_MAX_ENTITIES / 64u )
#define VECS_TOP_COUNT ( VECS_L2_COUNT / 64u )

constexpr uint64_t VECS_INVALID_ENTITY = UINT64_MAX;
constexpr uint32_t VECS_INVALID_INDEX  = UINT32_MAX;

// --------------------------------------------------------------------------
// Bit Intrinsics
// --------------------------------------------------------------------------

inline uint32_t veTzcnt( uint64_t v )
{
    assert( v != 0 );
    return ( uint32_t )__builtin_ctzll( v );
}

inline uint32_t vePopcnt( uint64_t v )
{
    return ( uint32_t )__builtin_popcountll( v );
}

// --------------------------------------------------------------------------
// Entity
// --------------------------------------------------------------------------

typedef uint64_t veEntity;

inline veEntity veMakeEntity( uint32_t index, uint32_t generation )
{
    return ( ( uint64_t )generation << 32 ) | ( uint64_t )index;
}

inline uint32_t veEntityIndex( veEntity e )
{
    return ( uint32_t )( e & 0xFFFFFFFFu );
}

inline uint32_t veEntityGeneration( veEntity e )
{
    return ( uint32_t )( e >> 32 );
}

struct veEntityPool
{
    uint32_t* generations;
    uint32_t* freeList;
    uint32_t freeCount;
    uint32_t maxEntities;
    uint32_t alive;
};

inline veEntityPool* veCreateEntityPool( uint32_t maxEntities )
{
    veEntityPool* pool = ( veEntityPool* )std::malloc( sizeof( veEntityPool ) );
    assert( pool );
    pool->generations = ( uint32_t* )std::calloc( maxEntities, sizeof( uint32_t ) );
    pool->freeList = ( uint32_t* )std::malloc( maxEntities * sizeof( uint32_t ) );
    assert( pool->generations );
    assert( pool->freeList );
    pool->freeCount = maxEntities;
    pool->maxEntities = maxEntities;
    pool->alive = 0;
    for ( uint32_t i = 0; i < maxEntities; i++ )
    {
        pool->freeList[i] = maxEntities - i - 1;
    }
    return pool;
}

inline void veDestroyEntityPool( veEntityPool* pool )
{
    if ( !pool )
    {
        return;
    }
    std::free( pool->generations );
    std::free( pool->freeList );
    std::free( pool );
}

inline veEntity veEntityPoolCreate( veEntityPool* pool )
{
    assert( pool );
    if ( pool->freeCount == 0 )
    {
        return VECS_INVALID_ENTITY;
    }
    uint32_t index = pool->freeList[--pool->freeCount];
    pool->alive++;
    return veMakeEntity( index, pool->generations[index] );
}

inline void veEntityPoolDestroy( veEntityPool* pool, veEntity entity )
{
    assert( pool );
    uint32_t index = veEntityIndex( entity );
    assert( index < pool->maxEntities );
    assert( pool->generations[index] == veEntityGeneration( entity ) );
    assert( pool->freeCount < pool->maxEntities );
    pool->generations[index]++;
    pool->freeList[pool->freeCount++] = index;
    assert( pool->alive > 0 );
    pool->alive--;
}

inline bool veEntityPoolAlive( veEntityPool* pool, veEntity entity )
{
    assert( pool );
    uint32_t index = veEntityIndex( entity );
    if ( index >= pool->maxEntities )
    {
        return false;
    }
    return pool->generations[index] == veEntityGeneration( entity );
}

// --------------------------------------------------------------------------
// Bitfield
// --------------------------------------------------------------------------

struct veBitfield
{
    uint64_t topMasks[VECS_TOP_COUNT];
    uint64_t l2Masks[VECS_L2_COUNT];
};

inline void veBitfieldClearAll( veBitfield* bf )
{
    assert( bf );
    std::memset( bf, 0, sizeof( veBitfield ) );
}

inline void veBitfieldSet( veBitfield* bf, uint32_t index )
{
    assert( bf );
    assert( index < VECS_MAX_ENTITIES );
    uint32_t l2 = index >> 6;
    uint32_t bit = index & 63u;
    bf->l2Masks[l2] |= ( 1ULL << bit );
    bf->topMasks[l2 >> 6] |= ( 1ULL << ( l2 & 63u ) );
}

inline void veBitfieldUnset( veBitfield* bf, uint32_t index )
{
    assert( bf );
    assert( index < VECS_MAX_ENTITIES );
    uint32_t l2 = index >> 6;
    uint32_t bit = index & 63u;
    bf->l2Masks[l2] &= ~( 1ULL << bit );
    if ( bf->l2Masks[l2] == 0 )
    {
        bf->topMasks[l2 >> 6] &= ~( 1ULL << ( l2 & 63u ) );
    }
}

inline bool veBitfieldHas( const veBitfield* bf, uint32_t index )
{
    assert( bf );
    assert( index < VECS_MAX_ENTITIES );
    uint32_t l2 = index >> 6;
    uint32_t bit = index & 63u;
    return ( bf->l2Masks[l2] & ( 1ULL << bit ) ) != 0;
}

inline uint32_t veBitfieldCount( const veBitfield* bf )
{
    assert( bf );
    uint32_t total = 0;
    for ( uint32_t i = 0; i < VECS_L2_COUNT; i++ )
    {
        total += vePopcnt( bf->l2Masks[i] );
    }
    return total;
}

template< typename Fn >
inline void veBitfieldEach( const veBitfield* bf, Fn&& fn )
{
    assert( bf );
    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = bf->topMasks[ti];
        while ( top )
        {
            uint32_t tb = veTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = bf->l2Masks[l2Idx];
            while ( l2 )
            {
                uint32_t lb = veTzcnt( l2 );
                fn( l2Idx * 64u + lb );
                l2 &= l2 - 1;
            }
            top &= top - 1;
        }
    }
}

template< typename Fn >
inline void veBitfieldJoin( const veBitfield* a, const veBitfield* b, Fn&& fn )
{
    assert( a );
    assert( b );
    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = a->topMasks[ti] & b->topMasks[ti];
        while ( top )
        {
            uint32_t tb = veTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = a->l2Masks[l2Idx] & b->l2Masks[l2Idx];
            while ( l2 )
            {
                uint32_t lb = veTzcnt( l2 );
                fn( l2Idx * 64u + lb );
                l2 &= l2 - 1;
            }
            top &= top - 1;
        }
    }
}

// --------------------------------------------------------------------------
// Component Pool
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// World
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Query
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Singleton
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Command Buffer
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// SIMD
// --------------------------------------------------------------------------
