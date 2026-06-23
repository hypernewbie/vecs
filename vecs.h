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
#include <algorithm>
#include <type_traits>
#include <tuple>
#include <utility>
#include <cstdio>
#include <atomic>
#include <thread>

#ifndef VECS_NO_SIMD
    #if defined( __AVX2__ )
        #define VECS_AVX2 1
        #include <immintrin.h>
        #define VECS_SSE2 1
        #if defined( _MSC_VER ) || defined( _WIN32 )
            #include <intrin.h>
        #endif
    #elif defined( __SSE2__ ) || defined( _M_X64 ) || defined( _M_AMD64 )
        #define VECS_SSE2 1
        #include <emmintrin.h>
        #if defined( _MSC_VER ) || defined( _WIN32 )
            #include <intrin.h>
        #endif
    #elif defined( __ARM_NEON ) || defined( __aarch64__ )
        #define VECS_NEON 1
        #include <arm_neon.h>
    #endif
#endif

#ifndef VECS_MAX_ENTITIES
#define VECS_MAX_ENTITIES 65536
#endif

#ifndef VECS_MAX_COMPONENTS
#define VECS_MAX_COMPONENTS 256
#endif

#define VECS_L2_COUNT  ( VECS_MAX_ENTITIES / 64u )
#define VECS_TOP_COUNT ( VECS_L2_COUNT / 64u )

constexpr uint32_t VECS_SIGNATURE_WORDS = ( VECS_MAX_COMPONENTS + 63u ) / 64u;
constexpr uint64_t VECS_INVALID_ENTITY = UINT64_MAX;
constexpr uint32_t VECS_INVALID_INDEX  = UINT32_MAX;

inline uint32_t vecsNormalizeWorldCapacity( uint32_t maxEntities )
{
    if ( maxEntities == 0u )
    {
        return 1u;
    }
    return maxEntities > VECS_MAX_ENTITIES ? ( uint32_t )VECS_MAX_ENTITIES : maxEntities;
}

inline bool vecsComponentIdValid( uint32_t componentId )
{
    return componentId < VECS_MAX_COMPONENTS;
}

// --------------------------------------------------------------------------
// Bit Intrinsics
// --------------------------------------------------------------------------

// Raw speed for bit-scanning. Input MUST be non-zero (enforced by assert in debug).
inline uint32_t vecsTzcnt( uint64_t v )
{
    assert( v != 0 );
#if defined( _MSC_VER ) && !defined( __clang__ )
    uint32_t count = 0u;
    while ( ( v & 1ULL ) == 0ULL )
    {
        v >>= 1u;
        count++;
    }
    return count;
#else
    return ( uint32_t )__builtin_ctzll( v );
#endif
}

inline uint32_t vecsPopcnt( uint64_t v )
{
#if defined( _MSC_VER ) && !defined( __clang__ )
    uint32_t count = 0u;
    while ( v )
    {
        v &= ( v - 1u );
        count++;
    }
    return count;
#else
    return ( uint32_t )__builtin_popcountll( v );
#endif
}

// --------------------------------------------------------------------------
// Aligned Memory
// --------------------------------------------------------------------------

// Required for SIMD paths. Alignment MUST be a power of 2.
inline void* vecsAlignedAlloc( size_t size, size_t alignment )
{
    assert( alignment > 0 && ( alignment & ( alignment - 1 ) ) == 0 );
#if defined( _MSC_VER ) || defined( _WIN32 )
    return _aligned_malloc( size, alignment );
#elif defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
    return std::aligned_alloc( alignment, size );
#else
    void* ptr = nullptr;
    posix_memalign( &ptr, alignment, size );
    return ptr;
#endif
}

inline void vecsAlignedFree( void* ptr )
{
#if defined( _MSC_VER ) || defined( _WIN32 )
    _aligned_free( ptr );
#else
    std::free( ptr );
#endif
}

inline void* vecsAlignedRealloc( void* ptr, size_t size, size_t alignment )
{
#if defined( _MSC_VER ) || defined( _WIN32 )
    return _aligned_realloc( ptr, size, alignment );
#else
    void* newPtr = vecsAlignedAlloc( size, alignment );
    if ( newPtr && ptr )
    {
        std::memcpy( newPtr, ptr, size );
        vecsAlignedFree( ptr );
    }
    return newPtr;
#endif
}

// --------------------------------------------------------------------------
// Entity
// --------------------------------------------------------------------------

// 32-bit index + 32-bit generation. Safe recycling for ~4 billion entities.
typedef uint64_t vecsEntity;

inline vecsEntity vecsMakeEntity( uint32_t index, uint32_t generation )
{
    return ( ( uint64_t )generation << 32 ) | ( uint64_t )index;
}

inline uint32_t vecsEntityIndex( vecsEntity e )
{
    return ( uint32_t )( e & 0xFFFFFFFFu );
}

inline uint32_t vecsEntityGeneration( vecsEntity e )
{
    return ( uint32_t )( e >> 32 );
}

struct vecsEntityPool
{
    uint32_t* generations;
    uint8_t* allocated;
    uint32_t* freeList;
    // VECS_SIGNATURE_WORDS * 64-bit mask per entity. Makes destruction O(active_components) instead of O(total_types).
    uint64_t* signatures[VECS_SIGNATURE_WORDS];
    uint32_t freeCount;
    uint32_t maxEntities;
    uint32_t alive;
    // High-water mark: highest live index + 1. Bounds snapshot capture/restore to live range.
    uint32_t hiAllocated;
};

inline vecsEntityPool* vecsCreateEntityPool( uint32_t maxEntities )
{
    vecsEntityPool* pool = ( vecsEntityPool* )std::malloc( sizeof( vecsEntityPool ) );
    assert( pool );
    pool->generations = ( uint32_t* )std::calloc( maxEntities, sizeof( uint32_t ) );
    pool->allocated = ( uint8_t* )std::calloc( maxEntities, sizeof( uint8_t ) );
    pool->freeList = ( uint32_t* )std::malloc( maxEntities * sizeof( uint32_t ) );
    assert( pool->generations );
    assert( pool->allocated );
    assert( pool->freeList );

    for ( uint32_t i = 0; i < VECS_SIGNATURE_WORDS; i++ )
    {
        pool->signatures[i] = ( uint64_t* )std::calloc( maxEntities, sizeof( uint64_t ) );
        assert( pool->signatures[i] );
    }

    pool->freeCount = maxEntities;
    pool->maxEntities = maxEntities;
    pool->alive = 0;
    pool->hiAllocated = 0u;
    for ( uint32_t i = 0; i < maxEntities; i++ )
    {
        pool->freeList[i] = maxEntities - i - 1;
    }
    return pool;
}

inline void vecsDestroyEntityPool( vecsEntityPool* pool )
{
    if ( !pool )
    {
        return;
    }
    std::free( pool->allocated );
    std::free( pool->generations );
    std::free( pool->freeList );
    for ( uint32_t i = 0; i < VECS_SIGNATURE_WORDS; i++ )
    {
        std::free( pool->signatures[i] );
    }
    std::free( pool );
}

inline vecsEntity vecsEntityPoolCreate( vecsEntityPool* pool )
{
    assert( pool );
    if ( pool->freeCount == 0 )
    {
        return VECS_INVALID_ENTITY;
    }
    uint32_t index = pool->freeList[--pool->freeCount];
    assert( !pool->allocated[index] );
    pool->allocated[index] = 1u;
    pool->alive++;
    if ( index + 1u > pool->hiAllocated ) pool->hiAllocated = index + 1u;
    return vecsMakeEntity( index, pool->generations[index] );
}

inline void vecsEntityPoolDestroy( vecsEntityPool* pool, vecsEntity entity )
{
    assert( pool );
    uint32_t index = vecsEntityIndex( entity );
    assert( index < pool->maxEntities );
    if ( index >= pool->maxEntities ) return;
    if ( pool->generations[index] != vecsEntityGeneration( entity ) ) return;
    if ( !pool->allocated[index] ) return;
    assert( pool->freeCount < pool->maxEntities );
    pool->allocated[index] = 0u;
    pool->generations[index]++;
    pool->freeList[pool->freeCount++] = index;
    assert( pool->alive > 0 );
    if ( pool->alive > 0u ) pool->alive--;
    // Scan-back: freeList is LIFO so the freed slot is usually re-allocated before
    // shrinking, keeping this O(1) amortised. Worst case is one full scan on world shrink.
    if ( index + 1u == pool->hiAllocated )
    {
        while ( pool->hiAllocated > 0u && !pool->allocated[pool->hiAllocated - 1u] ) pool->hiAllocated--;
    }
}

inline bool vecsEntityPoolAlive( vecsEntityPool* pool, vecsEntity entity )
{
    assert( pool );
    uint32_t index = vecsEntityIndex( entity );
    if ( index >= pool->maxEntities )
    {
        return false;
    }
    if ( !pool->allocated[index] )
    {
        return false;
    }
    return pool->generations[index] == vecsEntityGeneration( entity );
}

// --------------------------------------------------------------------------
// Bitfield
// --------------------------------------------------------------------------

// Hierarchical bitmask. Top level jumps over 4096 empty entities in a single check.
struct vecsBitfield
{
    uint64_t topMasks[VECS_TOP_COUNT];
    uint64_t l2Masks[VECS_L2_COUNT];
};

inline void vecsBitfieldClearAll( vecsBitfield* bf )
{
    assert( bf );
    std::memset( bf, 0, sizeof( vecsBitfield ) );
}

inline void vecsBitfieldSet( vecsBitfield* bf, uint32_t index )
{
    assert( bf );
    assert( index < VECS_MAX_ENTITIES );
    uint32_t l2 = index >> 6;
    uint32_t bit = index & 63u;
    bf->l2Masks[l2] |= ( 1ULL << bit );
    bf->topMasks[l2 >> 6] |= ( 1ULL << ( l2 & 63u ) );
}

inline void vecsBitfieldUnset( vecsBitfield* bf, uint32_t index )
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

inline bool vecsBitfieldHas( const vecsBitfield* bf, uint32_t index )
{
    assert( bf );
    assert( index < VECS_MAX_ENTITIES );
    uint32_t l2 = index >> 6;
    uint32_t bit = index & 63u;
    return ( bf->l2Masks[l2] & ( 1ULL << bit ) ) != 0;
}

inline uint32_t vecsBitfieldCount( const vecsBitfield* bf )
{
    assert( bf );
    uint32_t total = 0;
    for ( uint32_t i = 0; i < VECS_L2_COUNT; i++ )
    {
        total += vecsPopcnt( bf->l2Masks[i] );
    }
    return total;
}

template< typename Fn >
inline void vecsBitfieldEach( const vecsBitfield* bf, Fn&& fn )
{
    assert( bf );
    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = bf->topMasks[ti];
        while ( top )
        {
            uint32_t tb = vecsTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = bf->l2Masks[l2Idx];
            while ( l2 )
            {
                uint32_t lb = vecsTzcnt( l2 );
                fn( l2Idx * 64u + lb );
                l2 &= l2 - 1;
            }
            top &= top - 1;
        }
    }
}

template< typename Fn >
inline void vecsBitfieldJoin( const vecsBitfield* a, const vecsBitfield* b, Fn&& fn )
{
    assert( a );
    assert( b );
    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = a->topMasks[ti] & b->topMasks[ti];
        while ( top )
        {
            uint32_t tb = vecsTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = a->l2Masks[l2Idx] & b->l2Masks[l2Idx];
            while ( l2 )
            {
                uint32_t lb = vecsTzcnt( l2 );
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

// Hybrid Sparse-Set + Bitfield. SIMD queries use the bitfield; lookups use the sparse set.
struct vecsPool
{
    vecsBitfield bitfield;
    uint32_t* sparse;
    uint32_t* denseEntities;
    uint8_t* denseData;
    uint32_t count;
    uint32_t capacity;
    uint32_t stride;
    uint32_t alignment;
    bool noData; // Optimised path for Zero-Byte Tags.
    void ( *destructor )( void* );
    void ( *moveCtor )( void*, void* );
    void ( *copyCtor )( void*, const void* );

    // Seqlock. Even = stable, odd = writer in progress. atomic_init after malloc.
    std::atomic<uint64_t> gen;
    // High-water mark: highest live entity index + 1. Bounds snapshot capture/restore.
    uint32_t hiSparse;
};

inline void vecsPoolGrow( vecsPool* pool )
{
    assert( pool );
    uint32_t newCapacity = pool->capacity ? pool->capacity * 2u : 64u;
    if ( newCapacity > VECS_MAX_ENTITIES )
    {
        newCapacity = VECS_MAX_ENTITIES;
    }
    assert( newCapacity > pool->capacity );
    uint32_t* newDenseEntities = ( uint32_t* )std::realloc( pool->denseEntities, newCapacity * sizeof( uint32_t ) );
    assert( newDenseEntities );
    pool->denseEntities = newDenseEntities;

    if ( !pool->noData )
    {
        uint8_t* newDenseData = nullptr;
        if ( pool->alignment > 8 )
        {
            newDenseData = ( uint8_t* )vecsAlignedAlloc( ( size_t )newCapacity * pool->stride, pool->alignment );
        }
        else
        {
            newDenseData = ( uint8_t* )std::malloc( ( size_t )newCapacity * pool->stride );
        }
        assert( newDenseData );

        if ( pool->moveCtor )
        {
            for ( uint32_t i = 0; i < pool->count; i++ )
            {
                pool->moveCtor( newDenseData + ( size_t )i * pool->stride, pool->denseData + ( size_t )i * pool->stride );
                if ( pool->destructor )
                {
                    pool->destructor( pool->denseData + ( size_t )i * pool->stride );
                }
            }
        }
        else
        {
            std::memcpy( newDenseData, pool->denseData, ( size_t )pool->count * pool->stride );
        }

        if ( pool->alignment > 8 )
        {
            vecsAlignedFree( pool->denseData );
        }
        else
        {
            std::free( pool->denseData );
        }
        pool->denseData = newDenseData;
    }
    pool->capacity = newCapacity;
}

inline vecsPool* vecsCreatePool( uint32_t maxEntities, uint32_t stride, uint32_t alignment = 8u, void ( *dtor )( void* ) = nullptr, void ( *moveC )( void*, void* ) = nullptr, void ( *copyC )( void*, const void* ) = nullptr, bool noData = false )
{
    vecsPool* pool = ( vecsPool* )std::malloc( sizeof( vecsPool ) );
    assert( pool );
    pool->sparse = ( uint32_t* )std::malloc( maxEntities * sizeof( uint32_t ) );
    pool->denseEntities = ( uint32_t* )std::malloc( 64u * sizeof( uint32_t ) );
    pool->denseData = nullptr;
    if ( !noData )
    {
        if ( alignment > 8 )
        {
            pool->denseData = ( uint8_t* )vecsAlignedAlloc( ( size_t )64u * stride, alignment );
        }
        else
        {
            pool->denseData = ( uint8_t* )std::malloc( ( size_t )64u * stride );
        }
    }
    assert( pool->sparse );
    assert( pool->denseEntities );
    if ( !noData )
    {
        assert( pool->denseData );
    }
    for ( uint32_t i = 0; i < maxEntities; i++ )
    {
        pool->sparse[i] = VECS_INVALID_INDEX;
    }
    vecsBitfieldClearAll( &pool->bitfield );
    pool->count = 0;
    pool->capacity = 64u;
    pool->stride = stride;
    pool->alignment = alignment;
    pool->noData = noData;
    pool->destructor = dtor;
    pool->moveCtor = moveC;
    pool->copyCtor = copyC;
    std::atomic_init( &pool->gen, 0ull );
    pool->hiSparse = 0u;
    return pool;
}

inline void vecsDestroyPool( vecsPool* pool )
{
    if ( !pool )
    {
        return;
    }
    if ( pool->destructor && !pool->noData )
    {
        for ( uint32_t i = 0; i < pool->count; i++ )
        {
            pool->destructor( pool->denseData + ( size_t )i * pool->stride );
        }
    }
    std::free( pool->sparse );
    std::free( pool->denseEntities );
    if ( pool->denseData )
    {
        if ( pool->alignment > 8 )
        {
            vecsAlignedFree( pool->denseData );
        }
        else
        {
            std::free( pool->denseData );
        }
    }
    std::free( pool );
}

inline void* vecsPoolSet( vecsPool* pool, uint32_t entityIndex, const void* data )
{
    assert( pool );
    assert( entityIndex < VECS_MAX_ENTITIES );
    assert( !vecsBitfieldHas( &pool->bitfield, entityIndex ) );
    assert( pool->noData || data != nullptr );
    if ( pool->count == pool->capacity )
    {
        vecsPoolGrow( pool );
    }
    uint32_t denseIdx = pool->count++;
    pool->sparse[entityIndex] = denseIdx;
    pool->denseEntities[denseIdx] = entityIndex;
    uint8_t* dst = nullptr;
    if ( !pool->noData )
    {
        dst = pool->denseData + ( size_t )denseIdx * pool->stride;
        if ( pool->copyCtor )
        {
            pool->copyCtor( dst, data );
        }
        else
        {
            std::memcpy( dst, data, pool->stride );
        }
    }
    vecsBitfieldSet( &pool->bitfield, entityIndex );
    if ( entityIndex + 1u > pool->hiSparse ) pool->hiSparse = entityIndex + 1u;
    return dst;
}

inline void vecsPoolUnset( vecsPool* pool, uint32_t entityIndex )
{
    assert( pool );
    assert( entityIndex < VECS_MAX_ENTITIES );
    assert( vecsBitfieldHas( &pool->bitfield, entityIndex ) );
    uint32_t denseIdx = pool->sparse[entityIndex];
    uint32_t lastIdx = pool->count - 1u;
    if ( !pool->noData )
    {
        uint8_t* removePtr = pool->denseData + ( size_t )denseIdx * pool->stride;
        if ( pool->destructor )
        {
            pool->destructor( removePtr );
        }
        if ( denseIdx != lastIdx )
        {
            uint8_t* lastPtr = pool->denseData + ( size_t )lastIdx * pool->stride;
            if ( pool->moveCtor )
            {
                pool->moveCtor( removePtr, lastPtr );
                if ( pool->destructor )
                {
                    pool->destructor( lastPtr );
                }
            }
            else
            {
                std::memcpy( removePtr, lastPtr, pool->stride );
            }
        }
    }
    if ( denseIdx != lastIdx )
    {
        uint32_t movedEntity = pool->denseEntities[lastIdx];
        pool->denseEntities[denseIdx] = movedEntity;
        pool->sparse[movedEntity] = denseIdx;
    }
    vecsBitfieldUnset( &pool->bitfield, entityIndex );
    pool->sparse[entityIndex] = VECS_INVALID_INDEX;
    pool->count--;
    if ( entityIndex + 1u == pool->hiSparse )
    {
        while ( pool->hiSparse > 0u && pool->sparse[pool->hiSparse - 1u] == VECS_INVALID_INDEX ) pool->hiSparse--;
    }
}

inline void* vecsPoolGet( vecsPool* pool, uint32_t entityIndex )
{
    assert( pool );
    assert( entityIndex < VECS_MAX_ENTITIES );
    if ( !vecsBitfieldHas( &pool->bitfield, entityIndex ) )
    {
        return nullptr;
    }
    if ( pool->noData )
    {
        return nullptr;
    }
    uint32_t denseIdx = pool->sparse[entityIndex];
    return pool->denseData + ( size_t )denseIdx * pool->stride;
}

inline bool vecsPoolHas( const vecsPool* pool, uint32_t entityIndex )
{
    assert( pool );
    assert( entityIndex < VECS_MAX_ENTITIES );
    return vecsBitfieldHas( &pool->bitfield, entityIndex );
}

// --------------------------------------------------------------------------
// Observer (Reactive Events)
// --------------------------------------------------------------------------

struct vecsWorld;

struct vecsObserver
{
    void ( *callback )( vecsWorld*, vecsEntity, void* );
    uint32_t componentId;
    bool onAdd;
};

struct vecsObserverList
{
    vecsObserver* observers;
    uint32_t count;
    uint32_t capacity;
};

inline void vecsObserverListGrow( vecsObserverList* list )
{
    assert( list );
    uint32_t cap = list->capacity ? list->capacity * 2u : 8u;
    vecsObserver* ptr = ( vecsObserver* )std::realloc( list->observers, cap * sizeof( vecsObserver ) );
    assert( ptr );
    list->observers = ptr;
    list->capacity = cap;
}

inline void vecsObserverListDestroy( vecsObserverList* list )
{
    assert( list );
    if ( list->observers )
    {
        std::free( list->observers );
        list->observers = nullptr;
    }
    list->count = 0;
    list->capacity = 0;
}

inline void vecsAddObserver( vecsObserverList* list, uint32_t componentId, void ( *callback )( vecsWorld*, vecsEntity, void* ), bool onAdd )
{
    assert( list );
    assert( callback );
    assert( componentId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( componentId ) )
    {
        return;
    }
    if ( list->count == list->capacity )
    {
        vecsObserverListGrow( list );
    }
    assert( list->count <= list->capacity );
    vecsObserver& obs = list->observers[list->count++];
    obs.callback = callback;
    obs.componentId = componentId;
    obs.onAdd = onAdd;
    assert( list->count <= list->capacity );
}

// --------------------------------------------------------------------------
// Relationships (ChildOf)
// --------------------------------------------------------------------------

struct vecsChildOf
{
    vecsEntity parent;
};

struct vecsRelationshipData
{
    vecsEntity* parents;
    vecsEntity** children;
    uint32_t* childCounts;
    uint32_t* childCapacities;
    uint32_t maxEntities;
};

inline vecsRelationshipData* vecsCreateRelationships( uint32_t maxEntities )
{
    assert( maxEntities > 0u );
    assert( maxEntities <= VECS_MAX_ENTITIES );
    vecsRelationshipData* rel = ( vecsRelationshipData* )std::malloc( sizeof( vecsRelationshipData ) );
    assert( rel );
    rel->parents = ( vecsEntity* )std::calloc( maxEntities, sizeof( vecsEntity ) );
    rel->children = ( vecsEntity** )std::calloc( maxEntities, sizeof( vecsEntity* ) );
    rel->childCounts = ( uint32_t* )std::calloc( maxEntities, sizeof( uint32_t ) );
    rel->childCapacities = ( uint32_t* )std::calloc( maxEntities, sizeof( uint32_t ) );
    assert( rel->parents );
    assert( rel->children );
    assert( rel->childCounts );
    assert( rel->childCapacities );
    rel->maxEntities = maxEntities;
    for ( uint32_t i = 0; i < maxEntities; i++ )
    {
        rel->parents[i] = VECS_INVALID_ENTITY;
    }
    return rel;
}

inline void vecsDestroyRelationships( vecsRelationshipData* rel )
{
    assert( rel );
    if ( !rel )
    {
        return;
    }
    std::free( rel->parents );
    if ( rel->children )
    {
        for ( uint32_t i = 0; i < rel->maxEntities; i++ )
        {
            std::free( rel->children[i] );
        }
        std::free( rel->children );
    }
    std::free( rel->childCounts );
    std::free( rel->childCapacities );
    std::free( rel );
}

inline void vecsRemoveChildFromParent( vecsRelationshipData* rel, uint32_t parentIdx, vecsEntity child )
{
    assert( rel );
    assert( parentIdx < rel->maxEntities );
    assert( rel->childCounts[parentIdx] <= rel->childCapacities[parentIdx] );
    uint32_t count = rel->childCounts[parentIdx];
    for ( uint32_t i = 0; i < count; i++ )
    {
        if ( rel->children[parentIdx][i] == child )
        {
            uint32_t last = count - 1u;
            rel->children[parentIdx][i] = rel->children[parentIdx][last];
            rel->childCounts[parentIdx] = last;
            return;
        }
    }
}

inline void vecsSetParent( vecsRelationshipData* rel, vecsEntity child, vecsEntity parent )
{
    assert( rel );
    uint32_t childIdx = vecsEntityIndex( child );
    assert( childIdx < rel->maxEntities );

    vecsEntity oldParent = rel->parents[childIdx];
    if ( oldParent == parent )
    {
        return;
    }

    if ( oldParent != VECS_INVALID_ENTITY )
    {
        uint32_t oldParentIdx = vecsEntityIndex( oldParent );
        assert( oldParentIdx < rel->maxEntities );
        vecsRemoveChildFromParent( rel, oldParentIdx, child );
    }

    rel->parents[childIdx] = parent;

    if ( parent == VECS_INVALID_ENTITY )
    {
        return;
    }

    uint32_t parentIdx = vecsEntityIndex( parent );
    assert( parentIdx < rel->maxEntities );
    assert( rel->childCounts[parentIdx] <= rel->childCapacities[parentIdx] );
    if ( rel->childCounts[parentIdx] >= rel->childCapacities[parentIdx] )
    {
        uint32_t newCap = rel->childCapacities[parentIdx] ? rel->childCapacities[parentIdx] * 2u : 4u;
        vecsEntity* newChildren = ( vecsEntity* )std::realloc( rel->children[parentIdx], ( size_t )newCap * sizeof( vecsEntity ) );
        assert( newChildren );
        rel->children[parentIdx] = newChildren;
        rel->childCapacities[parentIdx] = newCap;
        assert( rel->childCounts[parentIdx] <= rel->childCapacities[parentIdx] );
    }
    rel->children[parentIdx][rel->childCounts[parentIdx]++] = child;
    assert( rel->childCounts[parentIdx] <= rel->childCapacities[parentIdx] );
}

inline void vecsClearChildren( vecsRelationshipData* rel, vecsEntity parent )
{
    assert( rel );
    uint32_t parentIdx = vecsEntityIndex( parent );
    assert( parentIdx < rel->maxEntities );
    if ( parentIdx >= rel->maxEntities )
    {
        return;
    }

    uint32_t count = rel->childCounts[parentIdx];
    for ( uint32_t i = 0; i < count; i++ )
    {
        vecsEntity child = rel->children[parentIdx][i];
        uint32_t childIdx = vecsEntityIndex( child );
        if ( childIdx < rel->maxEntities )
        {
            rel->parents[childIdx] = VECS_INVALID_ENTITY;
        }
    }
    rel->childCounts[parentIdx] = 0;
}

inline vecsEntity vecsGetParent( vecsRelationshipData* rel, vecsEntity child )
{
    assert( rel );
    uint32_t idx = vecsEntityIndex( child );
    assert( idx < rel->maxEntities );
    if ( idx >= rel->maxEntities )
    {
        return VECS_INVALID_ENTITY;
    }
    return rel->parents[idx];
}

inline uint32_t vecsGetChildCount( vecsRelationshipData* rel, vecsEntity parent )
{
    assert( rel );
    uint32_t idx = vecsEntityIndex( parent );
    assert( idx < rel->maxEntities );
    if ( idx >= rel->maxEntities )
    {
        return 0;
    }
    return rel->childCounts[idx];
}

inline vecsEntity vecsGetChild( vecsRelationshipData* rel, vecsEntity parent, uint32_t index )
{
    assert( rel );
    uint32_t idx = vecsEntityIndex( parent );
    assert( idx < rel->maxEntities );
    if ( idx >= rel->maxEntities || index >= rel->childCounts[idx] )
    {
        return VECS_INVALID_ENTITY;
    }
    return rel->children[idx][index];
}

inline void vecsSnapshotCaptureInto( vecsWorld* w, struct vecsWorldSnapshot* snap );

// --------------------------------------------------------------------------
// World
// --------------------------------------------------------------------------

// The central registry. Maintains pools, observers, and hierarchy.
struct vecsWorld
{
    // Trivially copyable; enforced at vecsSetSingleton via static_assert.
    struct vecsSingletonSlot
    {
        uint8_t* data;
        uint32_t size;
        void ( *destructor )( void* );
    };

    vecsEntityPool* entities;
    vecsPool* pools[VECS_MAX_COMPONENTS];
    vecsSingletonSlot singletons[VECS_MAX_COMPONENTS];
    vecsObserverList* observers;
    vecsRelationshipData* relationships;
    uint32_t maxEntities;
};

inline uint32_t& vecsGetTypeIdCounterMutable()
{
    static uint32_t counter = 0;
    return counter;
}

inline uint32_t vecsNextTypeId()
{
    return vecsGetTypeIdCounterMutable()++;
}

inline uint32_t vecsGetTypeIdCounter()
{
    return vecsGetTypeIdCounterMutable();
}

inline void vecsSetTypeIdCounter( uint32_t value )
{
    vecsGetTypeIdCounterMutable() = value;
}

template< typename T >
inline uint32_t vecsTypeIdRaw()
{
    static uint32_t id = vecsNextTypeId();
    return id;
}

template< typename T >
inline uint32_t vecsTypeId()
{
    using Raw = std::remove_cv_t<std::remove_reference_t<T>>;
    return vecsTypeIdRaw<Raw>();
}

// Invalid capacities are normalized in Release builds: 0 becomes 1 and
// oversized requests clamp to VECS_MAX_ENTITIES. Debug builds still assert.
inline vecsWorld* vecsCreateWorld( uint32_t maxEntities = VECS_MAX_ENTITIES )
{
    assert( maxEntities > 0u );
    assert( maxEntities <= VECS_MAX_ENTITIES );
    const uint32_t normalizedMaxEntities = vecsNormalizeWorldCapacity( maxEntities );
    vecsWorld* world = ( vecsWorld* )std::malloc( sizeof( vecsWorld ) );
    assert( world );
    world->entities = vecsCreateEntityPool( normalizedMaxEntities );
    world->maxEntities = normalizedMaxEntities;
    std::memset( world->pools, 0, sizeof( world->pools ) );
    std::memset( world->singletons, 0, sizeof( world->singletons ) );
    world->observers = ( vecsObserverList* )std::malloc( sizeof( vecsObserverList ) );
    assert( world->observers );
    world->observers->observers = nullptr;
    world->observers->count = 0;
    world->observers->capacity = 0;
    world->relationships = vecsCreateRelationships( normalizedMaxEntities );
    return world;
}

inline void vecsSetSignatureBit( vecsWorld* w, uint32_t entityIndex, uint32_t componentId )
{
    assert( w );
    assert( entityIndex < w->maxEntities );
    assert( componentId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( componentId ) )
    {
        return;
    }
    w->entities->signatures[componentId >> 6][entityIndex] |= ( 1ULL << ( componentId & 63u ) );
}

inline void vecsClearSignatureBit( vecsWorld* w, uint32_t entityIndex, uint32_t componentId )
{
    assert( w );
    assert( entityIndex < w->maxEntities );
    assert( componentId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( componentId ) )
    {
        return;
    }
    w->entities->signatures[componentId >> 6][entityIndex] &= ~( 1ULL << ( componentId & 63u ) );
}

inline void vecsNotifyObservers( vecsWorld* w, vecsEntity e, uint32_t componentId, bool onAdd, void* data )
{
    assert( w );
    assert( componentId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( componentId ) )
    {
        return;
    }
    if ( !w->observers )
    {
        return;
    }
    for ( uint32_t i = 0; i < w->observers->count; i++ )
    {
        vecsObserver& obs = w->observers->observers[i];
        if ( obs.onAdd == onAdd && obs.componentId == componentId )
        {
            obs.callback( w, e, data );
        }
    }
}

inline void vecsUnsetById( vecsWorld* w, vecsEntity e, uint32_t componentId )
{
    assert( w );
    assert( vecsEntityPoolAlive( w->entities, e ) );
    assert( componentId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( componentId ) )
    {
        return;
    }
    uint32_t idx = vecsEntityIndex( e );
    vecsPool* pool = w->pools[componentId];
    vecsClearSignatureBit( w, idx, componentId );
    vecsNotifyObservers( w, e, componentId, false, nullptr );
    if ( pool && vecsPoolHas( pool, idx ) )
    {
        vecsPoolUnset( pool, idx );
    }
}

inline bool vecsValidate( vecsWorld* w )
{
    if ( !w ) return true;

    vecsEntityPool* ep = w->entities;
    if ( ep->alive + ep->freeCount != ep->maxEntities ) { printf("Validation failed: alive + freeCount != maxEntities\n"); return false; }

    uint32_t allocatedCount = 0;
    for ( uint32_t i = 0; i < ep->maxEntities; i++ )
    {
        if ( ep->allocated[i] ) allocatedCount++;
    }
    if ( allocatedCount != ep->alive ) { printf("Validation failed: allocatedCount != alive (%u != %u)\n", allocatedCount, ep->alive); return false; }

    for ( uint32_t i = 0; i < ep->freeCount; i++ )
    {
        uint32_t fIdx = ep->freeList[i];
        if ( fIdx >= ep->maxEntities ) { printf("Validation failed: freeList index %u >= maxEntities\n", fIdx); return false; }
        if ( ep->allocated[fIdx] != 0 ) { printf("Validation failed: freeList index %u is allocated\n", fIdx); return false; }
    }

    for ( uint32_t c = 0; c < VECS_MAX_COMPONENTS; c++ )
    {
        vecsPool* pool = w->pools[c];
        if ( !pool ) continue;

        if ( pool->count > pool->capacity ) { printf("Validation failed: pool %u count > capacity\n", c); return false; }
        if ( pool->capacity > VECS_MAX_ENTITIES ) { printf("Validation failed: pool %u capacity > VECS_MAX_ENTITIES\n", c); return false; }

        uint32_t bitcount = vecsBitfieldCount( &pool->bitfield );
        if ( bitcount != pool->count ) { printf("Validation failed: pool %u bitcount != count (%u != %u)\n", c, bitcount, pool->count); return false; }

        for ( uint32_t i = 0; i < pool->count; i++ )
        {
            uint32_t entityIdx = pool->denseEntities[i];
            if ( entityIdx >= ep->maxEntities ) { printf("Validation failed: pool %u denseEntities[%u] = %u >= maxEntities\n", c, i, entityIdx); return false; }
            if ( pool->sparse[entityIdx] != i ) { printf("Validation failed: pool %u sparse[%u] != %u\n", c, entityIdx, i); return false; }
            if ( !ep->allocated[entityIdx] ) { printf("Validation failed: pool %u denseEntities[%u] = %u is not allocated\n", c, i, entityIdx); return false; }
        }

        for ( uint32_t entityIdx = 0; entityIdx < ep->maxEntities; entityIdx++ )
        {
            if ( pool->sparse[entityIdx] != VECS_INVALID_INDEX )
            {
                if ( pool->sparse[entityIdx] >= pool->count ) { printf("Validation failed: pool %u sparse[%u] = %u >= count %u\n", c, entityIdx, pool->sparse[entityIdx], pool->count); return false; }
            }
        }
    }

    for ( uint32_t e = 0; e < ep->maxEntities; e++ )
    {
        if ( !ep->allocated[e] )
        {
            bool hasSignature = false;
            for ( uint32_t word = 0; word < VECS_SIGNATURE_WORDS; word++ )
            {
                if ( ep->signatures[word][e] != 0 )
                {
                    hasSignature = true;
                    break;
                }
            }
            if ( hasSignature )
            {
                printf("Validation failed: dead entity %u has non-zero signature\n", e);
                return false;
            }
        }
        else
        {
            for ( uint32_t c = 0; c < VECS_MAX_COMPONENTS; c++ )
            {
                bool hasSig = ( ep->signatures[c / 64u][e] & ( 1ULL << ( c % 64u ) ) ) != 0;
                bool hasPool = w->pools[c] != nullptr;
                bool hasBit = false;
                if ( hasPool )
                {
                    hasBit = vecsBitfieldHas( &w->pools[c]->bitfield, e );
                }
                if ( hasSig != hasBit ) { printf("Validation failed: entity %u signature for component %u out of sync with bitfield (sig=%d, bit=%d)\n", e, c, hasSig, hasBit); return false; }
            }
        }
    }

    if ( w->relationships )
    {
        vecsRelationshipData* rel = w->relationships;
        for ( uint32_t e = 0; e < ep->maxEntities; e++ )
        {
            if ( !ep->allocated[e] )
            {
                if ( rel->parents[e] != VECS_INVALID_ENTITY ) { printf("Validation failed: dead entity %u still has a parent\n", e); return false; }
                if ( rel->childCounts[e] != 0 ) { printf("Validation failed: dead entity %u still has children\n", e); return false; }
                continue;
            }

            vecsEntity parent = rel->parents[e];
            if ( parent != VECS_INVALID_ENTITY )
            {
                uint32_t depth = 0;
                vecsEntity cursor = parent;
                while ( cursor != VECS_INVALID_ENTITY )
                {
                    if ( cursor == vecsMakeEntity( e, ep->generations[e] ) ) { printf("Validation failed: entity %u participates in a hierarchy cycle\n", e); return false; }
                    uint32_t cursorIdx = vecsEntityIndex( cursor );
                    if ( cursorIdx >= ep->maxEntities ) { printf("Validation failed: entity %u parent chain index >= maxEntities\n", e); return false; }
                    if ( !ep->allocated[cursorIdx] ) { printf("Validation failed: entity %u parent chain touches dead entity\n", e); return false; }
                    if ( ep->generations[cursorIdx] != vecsEntityGeneration( cursor ) ) { printf("Validation failed: entity %u parent chain generation mismatch\n", e); return false; }
                    if ( ++depth > ep->maxEntities ) { printf("Validation failed: entity %u parent chain exceeded world size\n", e); return false; }
                    cursor = rel->parents[cursorIdx];
                }

                uint32_t parentIdx = vecsEntityIndex( parent );
                if ( parentIdx >= ep->maxEntities ) { printf("Validation failed: entity %u parent index >= maxEntities\n", e); return false; }
                if ( !ep->allocated[parentIdx] ) { printf("Validation failed: entity %u parent is not allocated\n", e); return false; }
                if ( ep->generations[parentIdx] != vecsEntityGeneration( parent ) ) { printf("Validation failed: entity %u parent generation mismatch\n", e); return false; }

                bool found = false;
                for ( uint32_t i = 0; i < rel->childCounts[parentIdx]; i++ )
                {
                    if ( vecsEntityIndex( rel->children[parentIdx][i] ) == e )
                    {
                        found = true;
                        break;
                    }
                }
                if ( !found ) { printf("Validation failed: entity %u is not in parent %u child array\n", e, parentIdx); return false; }
            }

            if ( rel->childCounts[e] > rel->childCapacities[e] ) { printf("Validation failed: entity %u childCount > capacity\n", e); return false; }

            for ( uint32_t i = 0; i < rel->childCounts[e]; i++ )
            {
                vecsEntity child = rel->children[e][i];
                uint32_t childIdx = vecsEntityIndex( child );
                if ( childIdx >= ep->maxEntities ) { printf("Validation failed: entity %u child index >= maxEntities\n", e); return false; }
                if ( !ep->allocated[childIdx] ) { printf("Validation failed: entity %u child %u is not allocated\n", e, childIdx); return false; }
                if ( ep->generations[childIdx] != vecsEntityGeneration( child ) ) { printf("Validation failed: entity %u child generation mismatch\n", e); return false; }
                if ( rel->parents[childIdx] != vecsMakeEntity( e, ep->generations[e] ) ) { printf("Validation failed: entity %u child's parent is not %u\n", e, e); return false; }
                for ( uint32_t j = i + 1; j < rel->childCounts[e]; j++ )
                {
                    if ( rel->children[e][j] == child ) { printf("Validation failed: entity %u has duplicate child registration\n", e); return false; }
                }
            }
        }
    }

    return true;
}

inline void vecsDestroyWorld( vecsWorld* world )
{
    assert( world );
    if ( !world )
    {
        return;
    }
    for ( uint32_t i = 0; i < VECS_MAX_COMPONENTS; i++ )
    {
        vecsDestroyPool( world->pools[i] );
        if ( world->singletons[i].data )
        {
            if ( world->singletons[i].destructor )
            {
                world->singletons[i].destructor( world->singletons[i].data );
            }
            std::free( world->singletons[i].data );
        }
    }
    if ( world->observers )
    {
        vecsObserverListDestroy( world->observers );
        std::free( world->observers );
    }
    vecsDestroyRelationships( world->relationships );
    vecsDestroyEntityPool( world->entities );
    std::free( world );
}

// Reset the world to its initial empty state, preserving allocated memory.
inline void vecsClearWorld( vecsWorld* world )
{
    assert( world );
    if ( !world )
    {
        return;
    }

    for ( uint32_t i = 0; i < VECS_MAX_COMPONENTS; i++ )
    {
        vecsPool* pool = world->pools[i];
        if ( pool )
        {
            if ( pool->destructor && !pool->noData )
            {
                for ( uint32_t j = 0; j < pool->count; j++ )
                {
                    pool->destructor( pool->denseData + ( size_t )j * pool->stride );
                }
            }
            pool->count = 0;
            vecsBitfieldClearAll( &pool->bitfield );
            for ( uint32_t j = 0; j < world->maxEntities; j++ )
            {
                pool->sparse[j] = VECS_INVALID_INDEX;
            }
            pool->hiSparse = 0u;
        }

        if ( world->singletons[i].data )
        {
            if ( world->singletons[i].destructor )
            {
                world->singletons[i].destructor( world->singletons[i].data );
            }
            std::free( world->singletons[i].data );
            world->singletons[i].data = nullptr;
            world->singletons[i].size = 0;
            world->singletons[i].destructor = nullptr;
        }
    }

    vecsEntityPool* ep = world->entities;
    ep->freeCount = world->maxEntities;
    ep->alive = 0;
    ep->hiAllocated = 0u;
    for ( uint32_t i = 0; i < world->maxEntities; i++ )
    {
        ep->freeList[i] = world->maxEntities - i - 1;
    }
    std::memset( ep->allocated, 0, world->maxEntities * sizeof( uint8_t ) );
    for ( uint32_t i = 0; i < VECS_SIGNATURE_WORDS; i++ )
    {
        std::memset( ep->signatures[i], 0, world->maxEntities * sizeof( uint64_t ) );
    }
    for ( uint32_t i = 0; i < world->maxEntities; i++ )
    {
        ep->generations[i]++;
    }

    vecsRelationshipData* rel = world->relationships;
    for ( uint32_t i = 0; i < world->maxEntities; i++ )
    {
        rel->parents[i] = VECS_INVALID_ENTITY;
        if ( rel->children[i] )
        {
            std::free( rel->children[i] );
            rel->children[i] = nullptr;
        }
        rel->childCounts[i] = 0;
        rel->childCapacities[i] = 0;
    }
}

inline vecsEntity vecsCreate( vecsWorld* w )
{
    assert( w );
    return vecsEntityPoolCreate( w->entities );
}

inline bool vecsAlive( vecsWorld* w, vecsEntity e )
{
    assert( w );
    return vecsEntityPoolAlive( w->entities, e );
}

// Cascading destruction. Nuke the entity and its entire child subtree.
// Uses Entity Signatures to skip empty component pools at O(active_components) speed.
inline void vecsDestroyRecursive( vecsWorld* w, vecsEntity e )
{
    assert( w );
    assert( vecsEntityPoolAlive( w->entities, e ) );
    
    if ( w->relationships )
    {
        while ( vecsGetChildCount( w->relationships, e ) > 0u )
        {
            vecsEntity child = vecsGetChild( w->relationships, e, 0u );
            if ( child != VECS_INVALID_ENTITY && vecsAlive( w, child ) )
            {
                vecsDestroyRecursive( w, child );
            }
            else if ( child != VECS_INVALID_ENTITY )
            {
                vecsSetParent( w->relationships, child, VECS_INVALID_ENTITY );
            }
            else
            {
                break;
            }
        }
        vecsSetParent( w->relationships, e, VECS_INVALID_ENTITY );
        vecsClearChildren( w->relationships, e );
    }
    
    uint32_t entityIndex = vecsEntityIndex( e );
    for ( ;; )
    {
        uint32_t componentId = VECS_INVALID_INDEX;
        for ( uint32_t k = 0; k < VECS_SIGNATURE_WORDS; k++ )
        {
            uint64_t mask = w->entities->signatures[k][entityIndex];
            if ( mask )
            {
                componentId = ( k << 6 ) | vecsTzcnt( mask );
                break;
            }
        }
        if ( componentId == VECS_INVALID_INDEX )
        {
            break;
        }
        vecsUnsetById( w, e, componentId );
    }

    if ( w->relationships )
    {
        vecsSetParent( w->relationships, e, VECS_INVALID_ENTITY );
        vecsClearChildren( w->relationships, e );
    }

    vecsEntityPoolDestroy( w->entities, e );
}

inline void vecsDestroy( vecsWorld* w, vecsEntity e )
{
    assert( w );
    if ( !vecsAlive( w, e ) ) return;
    vecsDestroyRecursive( w, e );
}

inline uint32_t vecsCount( vecsWorld* w )
{
    assert( w );
    return w->entities->alive;
}

template< typename T >
inline vecsPool* vecsEnsurePool( vecsWorld* w )
{
    assert( w );
    uint32_t id = vecsTypeId<T>();
    assert( id < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( id ) )
    {
        return nullptr;
    }
    if ( !w->pools[id] )
    {
        constexpr bool kTag = std::is_empty<T>::value;
        void ( *dtor )( void* ) = nullptr;
        void ( *moveC )( void*, void* ) = nullptr;
        void ( *copyC )( void*, const void* ) = nullptr;
        if constexpr ( !kTag && !std::is_trivially_destructible<T>::value )
        {
            dtor = []( void* ptr ) { static_cast<T*>( ptr )->~T(); };
        }
        if constexpr ( !kTag && !std::is_trivially_copyable<T>::value )
        {
            moveC = []( void* dst, void* src ) { new ( dst ) T( std::move( *static_cast<T*>( src ) ) ); };
            copyC = []( void* dst, const void* src ) { new ( dst ) T( *static_cast<const T*>( src ) ); };
        }
        w->pools[id] = vecsCreatePool( w->maxEntities, kTag ? 0u : ( uint32_t )sizeof( T ), kTag ? 1u : ( uint32_t )alignof( T ), dtor, moveC, copyC, kTag );
    }
    return w->pools[id];
}

template< typename T >
inline T* vecsSet( vecsWorld* w, vecsEntity e, const T& val = {} )
{
    assert( w );
    assert( vecsAlive( w, e ) );
    uint32_t componentId = vecsTypeId<T>();
    assert( componentId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( componentId ) )
    {
        return nullptr;
    }
    vecsPool* pool = vecsEnsurePool<T>( w );
    if ( !pool )
    {
        return nullptr;
    }
    uint32_t idx = vecsEntityIndex( e );
    if ( vecsPoolHas( pool, idx ) )
    {
        if constexpr ( std::is_empty<T>::value )
        {
            static T tagValue = {};
            return &tagValue;
        }
        else
        {
            T* ptr = ( T* )vecsPoolGet( pool, idx );
            *ptr = val;
            return ptr;
        }
    }
    T* result = nullptr;
    if constexpr ( std::is_empty<T>::value )
    {
        vecsPoolSet( pool, idx, nullptr );
        static T tagValue = {};
        result = &tagValue;
    }
    else
    {
        result = ( T* )vecsPoolSet( pool, idx, &val );
    }

    vecsSetSignatureBit( w, idx, componentId );
    vecsNotifyObservers( w, e, componentId, true, result );
    return result;
}

template< typename T >
inline void vecsUnset( vecsWorld* w, vecsEntity e )
{
    assert( w );
    assert( vecsAlive( w, e ) );
    uint32_t componentId = vecsTypeId<T>();
    assert( componentId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( componentId ) )
    {
        return;
    }
    vecsPool* pool = vecsEnsurePool<T>( w );
    if ( !pool )
    {
        return;
    }
    uint32_t idx = vecsEntityIndex( e );
    assert( vecsPoolHas( pool, idx ) );
    ( void )pool;
    vecsUnsetById( w, e, componentId );
}

template< typename T >
inline T* vecsGet( vecsWorld* w, vecsEntity e )
{
    assert( w );
    assert( vecsAlive( w, e ) );
    uint32_t id = vecsTypeId<T>();
    assert( id < VECS_MAX_COMPONENTS );
    if ( id >= VECS_MAX_COMPONENTS || !w->pools[id] )
    {
        return nullptr;
    }
    vecsPool* pool = w->pools[id];
    if constexpr ( std::is_empty<T>::value )
    {
        if ( !vecsPoolHas( pool, vecsEntityIndex( e ) ) )
        {
            return nullptr;
        }
        static T tagValue = {};
        return &tagValue;
    }
    return ( T* )vecsPoolGet( pool, vecsEntityIndex( e ) );
}

template< typename T >
inline bool vecsHas( vecsWorld* w, vecsEntity e )
{
    assert( w );
    assert( vecsAlive( w, e ) );
    uint32_t id = vecsTypeId<T>();
    assert( id < VECS_MAX_COMPONENTS );
    if ( id >= VECS_MAX_COMPONENTS || !w->pools[id] )
    {
        return false;
    }
    return vecsPoolHas( w->pools[id], vecsEntityIndex( e ) );
}

inline vecsEntity vecsClone( vecsWorld* w, vecsEntity src )
{
    assert( w );
    assert( vecsAlive( w, src ) );
    vecsEntity dst = vecsCreate( w );
    if ( dst == VECS_INVALID_ENTITY )
    {
        return VECS_INVALID_ENTITY;
    }

    uint32_t srcIdx = vecsEntityIndex( src );
    uint32_t dstIdx = vecsEntityIndex( dst );
    for ( uint32_t i = 0; i < VECS_MAX_COMPONENTS; i++ )
    {
        vecsPool* pool = w->pools[i];
        if ( pool && vecsPoolHas( pool, srcIdx ) )
        {
            void* srcData = vecsPoolGet( pool, srcIdx );
            void* dstData = vecsPoolSet( pool, dstIdx, srcData );
            vecsSetSignatureBit( w, dstIdx, i );
            vecsNotifyObservers( w, dst, i, true, pool->noData ? nullptr : dstData );
        }
    }
    if ( w->relationships )
    {
        vecsEntity parent = vecsGetParent( w->relationships, src );
        if ( parent != VECS_INVALID_ENTITY )
        {
            vecsSetParent( w->relationships, dst, parent );
        }
    }
    return dst;
}

// Mass-spawn prefab instances. Prefab data is buffered to temporary memory first 
// to prevent pointer invalidation if the world grows during the batch.
inline void vecsInstantiateBatch( vecsWorld* w, vecsEntity prefab, vecsEntity* out, uint32_t count )
{
    assert( w );
    assert( vecsAlive( w, prefab ) );
    assert( out || count == 0u );

    struct vecsPrefabPoolCopy
    {
        vecsPool* pool;
        void* cachedData;
        uint32_t componentId;
    };

    vecsPrefabPoolCopy active[VECS_MAX_COMPONENTS] = {};
    uint32_t activeCount = 0;
    uint32_t prefabIdx = vecsEntityIndex( prefab );

    for ( uint32_t i = 0; i < VECS_MAX_COMPONENTS; i++ )
    {
        vecsPool* pool = w->pools[i];
        if ( pool && vecsPoolHas( pool, prefabIdx ) )
        {
            active[activeCount].pool = pool;
            active[activeCount].componentId = i;
            if ( !pool->noData )
            {
                active[activeCount].cachedData = std::malloc( pool->stride );
                assert( active[activeCount].cachedData );
                uint32_t dense = pool->sparse[prefabIdx];
                if ( pool->copyCtor )
                {
                    pool->copyCtor( active[activeCount].cachedData, pool->denseData + ( size_t )dense * pool->stride );
                }
                else
                {
                    std::memcpy( active[activeCount].cachedData, pool->denseData + ( size_t )dense * pool->stride, pool->stride );
                }
            }
            else
            {
                active[activeCount].cachedData = nullptr;
            }
            activeCount++;
        }
    }

    for ( uint32_t i = 0; i < count; i++ )
    {
        vecsEntity e = vecsCreate( w );
        out[i] = e;
        if ( e == VECS_INVALID_ENTITY )
        {
            continue;
        }

        uint32_t dstIdx = vecsEntityIndex( e );
        for ( uint32_t j = 0; j < activeCount; j++ )
        {
            void* dstData = vecsPoolSet( active[j].pool, dstIdx, active[j].cachedData );
            vecsSetSignatureBit( w, dstIdx, active[j].componentId );
            vecsNotifyObservers( w, e, active[j].componentId, true, active[j].pool->noData ? nullptr : dstData );
        }
        if ( w->relationships )
        {
            vecsEntity parent = vecsGetParent( w->relationships, prefab );
            if ( parent != VECS_INVALID_ENTITY )
            {
                vecsSetParent( w->relationships, e, parent );
            }
        }
    }

    for ( uint32_t j = 0; j < activeCount; j++ )
    {
        if ( active[j].cachedData )
        {
            if ( active[j].pool->destructor )
            {
                active[j].pool->destructor( active[j].cachedData );
            }
            std::free( active[j].cachedData );
        }
    }
}

// --------------------------------------------------------------------------
// Cached Query
// --------------------------------------------------------------------------

struct vecsQueryChunk
{
    uint32_t activeTi[VECS_TOP_COUNT];
    uint32_t count;
};

struct vecsQuery
{
    vecsBitfield withMask;
    vecsBitfield withoutMask;
    uint64_t readAccess[VECS_SIGNATURE_WORDS];
    uint64_t writeAccess[VECS_SIGNATURE_WORDS];
    uint32_t* withIds;
    uint32_t* withoutIds;
    uint32_t* optionalIds;
    uint32_t withCount;
    uint32_t withoutCount;
    uint32_t optionalCount;
    uint32_t withCapacity;
    uint32_t withoutCapacity;
    uint32_t optionalCapacity;
    bool impossible;
};

// One collected query match: the entity plus pointers into its live component data.
// Produced by vecsQueryCollect; see that function for the thread-safety / invalidation
// contract. Access a component with hit.get<T>().
template< typename... With >
struct vecsQueryHit
{
    vecsEntity entity;
    std::tuple<With*...> components;

    template< typename T >
    T& get() const { return *std::get<T*>( components ); }
};

inline vecsQuery* vecsCreateQuery()
{
    vecsQuery* q = ( vecsQuery* )std::malloc( sizeof( vecsQuery ) );
    assert( q );
    vecsBitfieldClearAll( &q->withMask );
    vecsBitfieldClearAll( &q->withoutMask );
    std::memset( q->readAccess, 0, sizeof( q->readAccess ) );
    std::memset( q->writeAccess, 0, sizeof( q->writeAccess ) );
    q->withIds = nullptr;
    q->withoutIds = nullptr;
    q->optionalIds = nullptr;
    q->withCount = 0;
    q->withoutCount = 0;
    q->optionalCount = 0;
    q->withCapacity = 0;
    q->withoutCapacity = 0;
    q->optionalCapacity = 0;
    q->impossible = false;
    return q;
}

inline void vecsDestroyQuery( vecsQuery* q )
{
    if ( !q )
    {
        return;
    }
    std::free( q->withIds );
    std::free( q->withoutIds );
    std::free( q->optionalIds );
    std::free( q );
}

inline void vecsQuerySetImpossible( vecsQuery* q )
{
    assert( q );
    q->impossible = true;
    vecsBitfieldClearAll( &q->withMask );
}

inline void vecsQueryMarkRead( vecsQuery* q, uint32_t typeId )
{
    assert( q );
    assert( typeId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( typeId ) )
    {
        return;
    }
    q->readAccess[typeId >> 6] |= ( 1ULL << ( typeId & 63u ) );
}

inline void vecsQueryMarkWrite( vecsQuery* q, uint32_t typeId )
{
    assert( q );
    assert( typeId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( typeId ) )
    {
        return;
    }
    q->writeAccess[typeId >> 6] |= ( 1ULL << ( typeId & 63u ) );
}

inline bool vecsQueryReads( const vecsQuery* q, uint32_t typeId )
{
    assert( q );
    assert( typeId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( typeId ) )
    {
        return false;
    }
    return ( q->readAccess[typeId >> 6] & ( 1ULL << ( typeId & 63u ) ) ) != 0;
}

inline bool vecsQueryWrites( const vecsQuery* q, uint32_t typeId )
{
    assert( q );
    assert( typeId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( typeId ) )
    {
        return false;
    }
    return ( q->writeAccess[typeId >> 6] & ( 1ULL << ( typeId & 63u ) ) ) != 0;
}

inline void vecsQueryRefreshWithMask( vecsWorld* w, vecsQuery* q )
{
    assert( w );
    assert( q );
    vecsBitfieldClearAll( &q->withMask );
    if ( q->impossible || q->withCount == 0 )
    {
        return;
    }

    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
    {
        q->withMask.topMasks[ti] = UINT64_MAX;
        for ( uint32_t i = 0; i < 64u && ( ti * 64u + i ) < VECS_L2_COUNT; i++ )
        {
            q->withMask.l2Masks[ti * 64u + i] = UINT64_MAX;
        }
    }

    for ( uint32_t i = 0; i < q->withCount; i++ )
    {
        vecsPool* pool = w->pools[q->withIds[i]];
        if ( !pool || pool->count == 0u )
        {
            vecsBitfieldClearAll( &q->withMask );
            return;
        }
        for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
        {
            q->withMask.topMasks[ti] &= pool->bitfield.topMasks[ti];
            for ( uint32_t l2 = 0; l2 < 64u && ( ti * 64u + l2 ) < VECS_L2_COUNT; l2++ )
            {
                q->withMask.l2Masks[ti * 64u + l2] &= pool->bitfield.l2Masks[ti * 64u + l2];
            }
        }
    }
}

inline void vecsQueryAddWith( vecsQuery* q, uint32_t typeId, vecsPool* pool )
{
    assert( q );
    assert( typeId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( typeId ) )
    {
        vecsQuerySetImpossible( q );
        return;
    }
    if ( q->withCount == q->withCapacity )
    {
        uint32_t cap = q->withCapacity ? q->withCapacity * 2u : 8u;
        uint32_t* ptr = ( uint32_t* )std::realloc( q->withIds, cap * sizeof( uint32_t ) );
        assert( ptr );
        q->withIds = ptr;
        q->withCapacity = cap;
        assert( q->withCount <= q->withCapacity );
    }
    q->withIds[q->withCount++] = typeId;
    assert( q->withCount <= q->withCapacity );
    if ( pool )
    {
        for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
        {
            q->withMask.topMasks[ti] &= pool->bitfield.topMasks[ti];
            q->withMask.l2Masks[ti * 64u] &= pool->bitfield.l2Masks[ti * 64u];
            for ( uint32_t i = 1; i < 64u && ( ti * 64u + i ) < VECS_L2_COUNT; i++ )
            {
                q->withMask.l2Masks[ti * 64u + i] &= pool->bitfield.l2Masks[ti * 64u + i];
            }
        }
    }
}

inline void vecsQueryAddWithout( vecsQuery* q, uint32_t typeId )
{
    assert( q );
    assert( typeId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( typeId ) )
    {
        return;
    }
    if ( q->withoutCount == q->withoutCapacity )
    {
        uint32_t cap = q->withoutCapacity ? q->withoutCapacity * 2u : 8u;
        uint32_t* ptr = ( uint32_t* )std::realloc( q->withoutIds, cap * sizeof( uint32_t ) );
        assert( ptr );
        q->withoutIds = ptr;
        q->withoutCapacity = cap;
        assert( q->withoutCount <= q->withoutCapacity );
    }
    q->withoutIds[q->withoutCount++] = typeId;
    assert( q->withoutCount <= q->withoutCapacity );
}

inline void vecsQueryAddOptional( vecsQuery* q, uint32_t typeId )
{
    assert( q );
    assert( typeId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( typeId ) )
    {
        return;
    }
    if ( q->optionalCount == q->optionalCapacity )
    {
        uint32_t cap = q->optionalCapacity ? q->optionalCapacity * 2u : 8u;
        uint32_t* ptr = ( uint32_t* )std::realloc( q->optionalIds, cap * sizeof( uint32_t ) );
        assert( ptr );
        q->optionalIds = ptr;
        q->optionalCapacity = cap;
        assert( q->optionalCount <= q->optionalCapacity );
    }
    q->optionalIds[q->optionalCount++] = typeId;
    assert( q->optionalCount <= q->optionalCapacity );
}

namespace vecsDetail
{

template< typename T >
inline void buildQueryWithType( vecsWorld* w, vecsQuery* q )
{
    assert( w );
    assert( q );
    using Raw = std::remove_cv_t<T>;
    uint32_t id = vecsTypeId<Raw>();
    assert( id < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( id ) )
    {
        vecsQuerySetImpossible( q );
        return;
    }
    vecsPool* pool = ( id < VECS_MAX_COMPONENTS ) ? w->pools[id] : nullptr;
    vecsQueryAddWith( q, id, pool );
    if constexpr ( std::is_const<T>::value )
    {
        vecsQueryMarkRead( q, id );
    }
    else
    {
        vecsQueryMarkWrite( q, id );
    }
}

template< typename... With >
inline void buildQueryWith( vecsWorld* w, vecsQuery* q )
{
    assert( w );
    assert( q );
    if constexpr ( sizeof...( With ) > 0 )
    {
        ( buildQueryWithType<With>( w, q ), ... );
    }
}

template< typename... Without >
inline void buildQueryWithout( vecsWorld* w, vecsQuery* q )
{
    assert( w );
    assert( q );
    ( void )w;
    if constexpr ( sizeof...( Without ) > 0 )
    {
        uint32_t ids[] = { vecsTypeId<Without>()... };
        for ( size_t i = 0; i < sizeof...( Without ); i++ )
        {
            vecsQueryAddWithout( q, ids[i] );
            vecsQueryMarkRead( q, ids[i] );
        }
    }
}

template< typename... Optional >
inline void buildQueryOptional( vecsWorld* w, vecsQuery* q )
{
    assert( w );
    assert( q );
    ( void )w;
    if constexpr ( sizeof...( Optional ) > 0 )
    {
        uint32_t ids[] = { vecsTypeId<Optional>()... };
        for ( size_t i = 0; i < sizeof...( Optional ); i++ )
        {
            vecsQueryAddOptional( q, ids[i] );
            vecsQueryMarkRead( q, ids[i] );
        }
    }
}

}

// Builds a cached intersection query for the given component types.
//
// The With... types nominated here form the query's identity: the internal
// withMask is intersected against each pool that exists at build time. For
// pools that don't exist yet the mask stays all-ones; the execution paths
// compensate by re-intersecting against current pool bitfields at runtime.
//
// CONTRACT: the With... list here must be a superset of (or identical to)
// the component types you will pass to vecsQueryEach / vecsQueryExecuteChunk.
// Passing a narrower set at build time and a wider set at execution time will
// make getData<> dereference VECS_INVALID_INDEX from an unowned sparse slot,
// causing memory corruption or an access violation.
//
// Invalid With types make the query impossible and all later execution returns
// empty results. Invalid Without/Optional ids are ignored.
//
// Destroy the returned query with vecsDestroyQuery().
template< typename... With >
inline vecsQuery* vecsBuildQuery( vecsWorld* w )
{
    assert( w );
    vecsQuery* q = vecsCreateQuery();
    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
    {
        q->withMask.topMasks[ti] = UINT64_MAX;
        for ( uint32_t i = 0; i < 64u && ( ti * 64u + i ) < VECS_L2_COUNT; i++ )
        {
            q->withMask.l2Masks[ti * 64u + i] = UINT64_MAX;
        }
    }
    vecsDetail::buildQueryWith<With...>( w, q );
    return q;
}

// --------------------------------------------------------------------------
// Query
// --------------------------------------------------------------------------

namespace vecsDetail
{

template< typename T >
inline vecsPool* getPool( vecsWorld* w )
{
    uint32_t id = vecsTypeId<T>();
    if ( id >= VECS_MAX_COMPONENTS )
    {
        return nullptr;
    }
    return w->pools[id];
}

template< typename T >
inline T* getData( vecsPool* pool, uint32_t entityIdx )
{
    if constexpr ( std::is_empty<T>::value )
    {
        ( void )pool;
        ( void )entityIdx;
        static T tagValue = {};
        return &tagValue;
    }
    uint32_t dense = pool->sparse[entityIdx];
    return ( T* )( pool->denseData + ( size_t )dense * pool->stride );
}

template< typename Tuple, typename Fn, size_t... I >
inline void forEachPoolImpl( Tuple& pools, Fn&& fn, std::index_sequence<I...> )
{
    ( fn( std::get<I>( pools ) ), ... );
}

template< typename Tuple, typename Fn >
inline void forEachPool( Tuple& pools, Fn&& fn )
{
    constexpr size_t N = std::tuple_size<Tuple>::value;
    forEachPoolImpl( pools, std::forward<Fn>( fn ), std::make_index_sequence<N>() );
}

template< typename... Components, typename Tuple, typename Fn, size_t... I >
inline void invokeJoin( vecsWorld* w, Tuple& pools, uint32_t entityIdx, Fn&& fn, std::index_sequence<I...> )
{
    vecsEntity entity = vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
    fn( entity, *getData<Components>( std::get<I>( pools ), entityIdx )... );
}

template< typename... With, typename WithTuple, typename OptionalTuple, typename Fn, size_t... I >
inline void invokeQueryCallback( vecsWorld* w, WithTuple& withPools, OptionalTuple& optionalPools, uint32_t entityIdx, vecsEntity entity, Fn&& fn, std::index_sequence<I...> )
{
    ( void )w;
    ( void )optionalPools;
    fn( entity, *getData<With>( std::get<I>( withPools ), entityIdx )... );
}

template< typename Tuple >
inline vecsEntity findFirstScalar( vecsWorld* w, Tuple& pools )
{
    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = UINT64_MAX;
        forEachPool( pools, [&]( vecsPool* pool ) { top &= pool->bitfield.topMasks[ti]; } );
        while ( top )
        {
            uint32_t tb = vecsTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = UINT64_MAX;
            forEachPool( pools, [&]( vecsPool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
            if ( l2 )
            {
                uint32_t lb = vecsTzcnt( l2 );
                uint32_t entityIdx = l2Idx * 64u + lb;
                return vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
            }
            top &= top - 1;
        }
    }
    return VECS_INVALID_ENTITY;
}

template< typename... Components, typename Tuple, typename Fn >
inline void eachJoinScalar( vecsWorld* w, Tuple& pools, Fn&& fn )
{
    constexpr size_t N = sizeof...( Components );
    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = UINT64_MAX;
        forEachPool( pools, [&]( vecsPool* pool ) { top &= pool->bitfield.topMasks[ti]; } );
        while ( top )
        {
            uint32_t tb = vecsTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = UINT64_MAX;
            forEachPool( pools, [&]( vecsPool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
            while ( l2 )
            {
                uint32_t lb = vecsTzcnt( l2 );
                uint32_t entityIdx = l2Idx * 64u + lb;
                invokeJoin<Components...>( w, pools, entityIdx, fn, std::make_index_sequence<N>() );
                l2 &= l2 - 1;
            }
            top &= top - 1;
        }
    }
}

} // namespace vecsDetail

enum vecsSimdLevel
{
    VECS_SIMD_SCALAR = 0,
    VECS_SIMD_SSE2   = 1,
    VECS_SIMD_AVX2   = 2,
    VECS_SIMD_NEON   = 3,
    VECS_SIMD_AUTO   = 4
};

inline vecsSimdLevel g_vecsSimdConfig = VECS_SIMD_AUTO;

inline vecsSimdLevel vecsRuntimeSimdSupported()
{
    if ( g_vecsSimdConfig != VECS_SIMD_AUTO )
    {
        return g_vecsSimdConfig;
    }
#if defined( VECS_AVX2 )
    static int cached = -1;
    if ( cached < 0 )
    {
        #if defined( _MSC_VER ) || defined( _WIN32 )
            int cpuInfo[4] = {};
            __cpuid( cpuInfo, 0 );
            if ( cpuInfo[0] >= 7 )
            {
                __cpuidex( cpuInfo, 7, 0 );
                cached = ( cpuInfo[1] & ( 1 << 5 ) ) ? ( int )VECS_SIMD_AVX2 : ( int )VECS_SIMD_SSE2;
            }
            else
            {
                __cpuid( cpuInfo, 1 );
                cached = ( cpuInfo[3] & ( 1 << 26 ) ) ? ( int )VECS_SIMD_SSE2 : ( int )VECS_SIMD_SCALAR;
            }
        #elif defined( __x86_64__ ) || defined( __i386__ )
            if ( __builtin_cpu_supports( "avx2" ) )
            {
                cached = ( int )VECS_SIMD_AVX2;
            }
            else if ( __builtin_cpu_supports( "sse2" ) )
            {
                cached = ( int )VECS_SIMD_SSE2;
            }
            else
            {
                cached = ( int )VECS_SIMD_SCALAR;
            }
        #else
            cached = ( int )VECS_SIMD_SCALAR;
        #endif
    }
    return ( vecsSimdLevel )cached;
#elif defined( VECS_SSE2 )
    static int cached = -1;
    if ( cached < 0 )
    {
        #if defined( _M_X64 ) || defined( _M_AMD64 )
            cached = ( int )VECS_SIMD_SSE2;
        #elif defined( _MSC_VER ) || defined( _WIN32 )
            int cpuInfo[4] = {};
            __cpuid( cpuInfo, 1 );
            cached = ( cpuInfo[3] & ( 1 << 26 ) ) ? ( int )VECS_SIMD_SSE2 : ( int )VECS_SIMD_SCALAR;
        #elif defined( __x86_64__ ) || defined( __i386__ )
            cached = __builtin_cpu_supports( "sse2" ) ? ( int )VECS_SIMD_SSE2 : ( int )VECS_SIMD_SCALAR;
        #else
            cached = ( int )VECS_SIMD_SCALAR;
        #endif
    }
    return ( vecsSimdLevel )cached;
#elif defined( VECS_NEON )
    return VECS_SIMD_NEON;
#else
    return VECS_SIMD_SCALAR;
#endif
}

namespace vecsDetail
{

#if defined( VECS_SSE2 )
template< typename Tuple, size_t... I >
inline __m128i andTopMasksSse( Tuple& pools, uint32_t ti, std::index_sequence<I...> )
{
    __m128i joined = _mm_set1_epi32( -1 );
    ( ( joined = _mm_and_si128( joined, _mm_loadu_si128( ( const __m128i* )&std::get<I>( pools )->bitfield.topMasks[ti] ) ) ), ... );
    return joined;
}
#endif

#if defined( VECS_NEON )
inline uint64x2_t vecsNeonLoad( const uint64_t* ptr ) { return vld1q_u64( ptr ); }

template< typename Tuple, size_t... I >
inline uint64x2_t andTopMasksNeon( Tuple& pools, uint32_t ti, std::index_sequence<I...> )
{
    uint64x2_t joined = vdupq_n_u64( UINT64_MAX );
    ( ( joined = vandq_u64( joined, vecsNeonLoad( &std::get<I>( pools )->bitfield.topMasks[ti] ) ) ), ... );
    return joined;
}
#endif

#if defined( VECS_AVX2 )
template< typename Tuple, size_t... I >
inline __m256i andTopMasksAvx2( Tuple& pools, uint32_t ti, std::index_sequence<I...> )
{
    __m256i joined = _mm256_set1_epi32( -1 );
    ( ( joined = _mm256_and_si256( joined, _mm256_loadu_si256( ( const __m256i* )&std::get<I>( pools )->bitfield.topMasks[ti] ) ) ), ... );
    return joined;
}
#endif

template< typename Tuple >
inline vecsEntity findFirstSimd( vecsWorld* w, Tuple& pools )
{
    constexpr size_t PoolCount = std::tuple_size<Tuple>::value;
    uint32_t ti = 0;
    vecsSimdLevel simdLevel = vecsRuntimeSimdSupported();
    
#if defined( VECS_AVX2 )
    if ( simdLevel == VECS_SIMD_AVX2 )
    {
        for ( ; ti + 3 < VECS_TOP_COUNT; ti += 4 )
        {
            __m256i joined = andTopMasksAvx2( pools, ti, std::make_index_sequence<PoolCount>() );
            if ( _mm256_testz_si256( joined, joined ) )
            {
                continue;
            }
            alignas( 32 ) uint64_t vals[4] = {};
            _mm256_store_si256( ( __m256i* )vals, joined );
            for ( uint32_t k = 0; k < 4u; k++ )
            {
                uint64_t top = vals[k];
                while ( top )
                {
                    uint32_t tb = vecsTzcnt( top );
                    uint32_t l2Idx = ( ti + k ) * 64u + tb;
                    uint64_t l2 = UINT64_MAX;
                    forEachPool( pools, [&]( vecsPool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
                    if ( l2 )
                    {
                        uint32_t lb = vecsTzcnt( l2 );
                        uint32_t entityIdx = l2Idx * 64u + lb;
                        return vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
                    }
                    top &= top - 1;
                }
            }
        }
    }
    else
#endif
#if defined( VECS_SSE2 ) || defined( VECS_NEON )
    if ( simdLevel == VECS_SIMD_SSE2 || simdLevel == VECS_SIMD_NEON )
    {
        for ( ; ti + 1 < VECS_TOP_COUNT; ti += 2 )
        {
            uint64_t vals[2] = {};
#if defined( VECS_SSE2 )
            __m128i joined = andTopMasksSse( pools, ti, std::make_index_sequence<PoolCount>() );
            __m128i zero = _mm_setzero_si128();
            __m128i cmp = _mm_cmpeq_epi32( joined, zero );
            if ( _mm_movemask_epi8( cmp ) == 0xFFFF )
            {
                continue;
            }
            alignas( 16 ) uint64_t alignedVals[2] = {};
            _mm_store_si128( ( __m128i* )alignedVals, joined );
            vals[0] = alignedVals[0];
            vals[1] = alignedVals[1];
#elif defined( VECS_NEON )
            uint64x2_t joined = andTopMasksNeon( pools, ti, std::make_index_sequence<PoolCount>() );
            vals[0] = vgetq_lane_u64( joined, 0 );
            vals[1] = vgetq_lane_u64( joined, 1 );
            if ( ( vals[0] | vals[1] ) == 0 )
            {
                continue;
            }
#endif
            for ( uint32_t k = 0; k < 2u; k++ )
            {
                uint64_t top = vals[k];
                while ( top )
                {
                    uint32_t tb = vecsTzcnt( top );
                    uint32_t l2Idx = ( ti + k ) * 64u + tb;
                    uint64_t l2 = UINT64_MAX;
                    forEachPool( pools, [&]( vecsPool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
                    if ( l2 )
                    {
                        uint32_t lb = vecsTzcnt( l2 );
                        uint32_t entityIdx = l2Idx * 64u + lb;
                        return vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
                    }
                    top &= top - 1;
                }
            }
        }
    }
#endif
    
    for ( ; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = UINT64_MAX;
        forEachPool( pools, [&]( vecsPool* pool ) { top &= pool->bitfield.topMasks[ti]; } );
        while ( top )
        {
            uint32_t tb = vecsTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = UINT64_MAX;
            forEachPool( pools, [&]( vecsPool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
            if ( l2 )
            {
                uint32_t lb = vecsTzcnt( l2 );
                uint32_t entityIdx = l2Idx * 64u + lb;
                return vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
            }
            top &= top - 1;
        }
    }
    return VECS_INVALID_ENTITY;
}

// Vectorised join. Intersects bitfields to jump over large gaps.
// Note: In 100% dense worlds, Scalar may be slightly faster due to lower setup overhead.
template< typename... Components, typename Tuple, typename Fn >
inline void eachJoinSimd( vecsWorld* w, Tuple& pools, Fn&& fn ){
    constexpr size_t N = sizeof...( Components );
    constexpr size_t PoolCount = std::tuple_size<Tuple>::value;
    uint32_t ti = 0;
    vecsSimdLevel simdLevel = vecsRuntimeSimdSupported();
    
#if defined( VECS_AVX2 )
    if ( simdLevel == VECS_SIMD_AVX2 )
    {
        for ( ; ti + 3 < VECS_TOP_COUNT; ti += 4 )
        {
            __m256i joined = andTopMasksAvx2( pools, ti, std::make_index_sequence<PoolCount>() );
            if ( _mm256_testz_si256( joined, joined ) )
            {
                continue;
            }
            alignas( 32 ) uint64_t vals[4] = {};
            _mm256_store_si256( ( __m256i* )vals, joined );
            for ( uint32_t k = 0; k < 4u; k++ )
            {
                uint64_t top = vals[k];
                while ( top )
                {
                    uint32_t tb = vecsTzcnt( top );
                    uint32_t l2Idx = ( ti + k ) * 64u + tb;
                    uint64_t l2 = UINT64_MAX;
                    forEachPool( pools, [&]( vecsPool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
                    while ( l2 )
                    {
                        uint32_t lb = vecsTzcnt( l2 );
                        uint32_t entityIdx = l2Idx * 64u + lb;
                        invokeJoin<Components...>( w, pools, entityIdx, fn, std::make_index_sequence<N>() );
                        l2 &= l2 - 1;
                    }
                    top &= top - 1;
                }
            }
        }
    }
    else
#endif
#if defined( VECS_SSE2 ) || defined( VECS_NEON )
    if ( simdLevel == VECS_SIMD_SSE2 || simdLevel == VECS_SIMD_NEON )
    {
        for ( ; ti + 1 < VECS_TOP_COUNT; ti += 2 )
        {
            uint64_t vals[2] = {};
#if defined( VECS_SSE2 )
            __m128i joined = andTopMasksSse( pools, ti, std::make_index_sequence<PoolCount>() );
            __m128i zero = _mm_setzero_si128();
            __m128i cmp = _mm_cmpeq_epi32( joined, zero );
            if ( _mm_movemask_epi8( cmp ) == 0xFFFF )
            {
                continue;
            }
            alignas( 16 ) uint64_t alignedVals[2] = {};
            _mm_store_si128( ( __m128i* )alignedVals, joined );
            vals[0] = alignedVals[0];
            vals[1] = alignedVals[1];
#elif defined( VECS_NEON )
            uint64x2_t joined = andTopMasksNeon( pools, ti, std::make_index_sequence<PoolCount>() );
            vals[0] = vgetq_lane_u64( joined, 0 );
            vals[1] = vgetq_lane_u64( joined, 1 );
            if ( ( vals[0] | vals[1] ) == 0 )
            {
                continue;
            }
#endif
            for ( uint32_t k = 0; k < 2u; k++ )
            {
                uint64_t top = vals[k];
                while ( top )
                {
                    uint32_t tb = vecsTzcnt( top );
                    uint32_t l2Idx = ( ti + k ) * 64u + tb;
                    uint64_t l2 = UINT64_MAX;
                    forEachPool( pools, [&]( vecsPool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
                    while ( l2 )
                    {
                        uint32_t lb = vecsTzcnt( l2 );
                        uint32_t entityIdx = l2Idx * 64u + lb;
                        invokeJoin<Components...>( w, pools, entityIdx, fn, std::make_index_sequence<N>() );
                        l2 &= l2 - 1;
                    }
                    top &= top - 1;
                }
            }
        }
    }
#endif
    
    for ( ; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = UINT64_MAX;
        forEachPool( pools, [&]( vecsPool* pool ) { top &= pool->bitfield.topMasks[ti]; } );
        while ( top )
        {
            uint32_t tb = vecsTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = UINT64_MAX;
            forEachPool( pools, [&]( vecsPool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
            while ( l2 )
            {
                uint32_t lb = vecsTzcnt( l2 );
                uint32_t entityIdx = l2Idx * 64u + lb;
                invokeJoin<Components...>( w, pools, entityIdx, fn, std::make_index_sequence<N>() );
                l2 &= l2 - 1;
            }
            top &= top - 1;
        }
    }
}

} // namespace vecsDetail

template< typename... With >
inline vecsEntity vecsQueryFirstMatch( vecsWorld* w, vecsQuery* q )
{
    assert( w );
    assert( q );
    vecsQueryRefreshWithMask( w, q );

    if ( q->impossible || q->withCount == 0 )
    {
        return VECS_INVALID_ENTITY;
    }

    uint32_t ti = 0;
    vecsSimdLevel simdLevel = vecsRuntimeSimdSupported();
    
#if defined( VECS_AVX2 )
    if ( simdLevel == VECS_SIMD_AVX2 )
    {
        for ( ; ti + 3 < VECS_TOP_COUNT; ti += 4 )
        {
            __m256i joined = _mm256_loadu_si256( ( const __m256i* )&q->withMask.topMasks[ti] );
            if ( _mm256_testz_si256( joined, joined ) )
            {
                continue;
            }
            alignas( 32 ) uint64_t vals[4] = {};
            _mm256_store_si256( ( __m256i* )vals, joined );
            for ( uint32_t k = 0; k < 4u; k++ )
            {
                uint64_t top = vals[k];
                while ( top )
                {
                    uint32_t tb = vecsTzcnt( top );
                    uint32_t l2Idx = ( ti + k ) * 64u + tb;
                    uint64_t l2 = q->withMask.l2Masks[l2Idx];
                    
                    for ( uint32_t i = 0; i < q->withoutCount; i++ )
                    {
                        vecsPool* wp = w->pools[q->withoutIds[i]];
                        if ( wp )
                        {
                            l2 &= ~wp->bitfield.l2Masks[l2Idx];
                        }
                    }
                    
                    if ( l2 )
                    {
                        uint32_t lb = vecsTzcnt( l2 );
                        uint32_t entityIdx = l2Idx * 64u + lb;
                        return vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
                    }
                    top &= top - 1;
                }
            }
        }
    }
    else
#endif
#if defined( VECS_SSE2 ) || defined( VECS_NEON )
    if ( simdLevel == VECS_SIMD_SSE2 || simdLevel == VECS_SIMD_NEON )
    {
        for ( ; ti + 1 < VECS_TOP_COUNT; ti += 2 )
        {
            uint64_t vals[2] = {};
#if defined( VECS_SSE2 )
            __m128i joined = _mm_loadu_si128( ( const __m128i* )&q->withMask.topMasks[ti] );
            __m128i zero = _mm_setzero_si128();
            __m128i cmp = _mm_cmpeq_epi32( joined, zero );
            if ( _mm_movemask_epi8( cmp ) == 0xFFFF )
            {
                continue;
            }
            alignas( 16 ) uint64_t alignedVals[2] = {};
            _mm_store_si128( ( __m128i* )alignedVals, joined );
            vals[0] = alignedVals[0];
            vals[1] = alignedVals[1];
#elif defined( VECS_NEON )
            uint64x2_t joined = vld1q_u64( &q->withMask.topMasks[ti] );
            vals[0] = vgetq_lane_u64( joined, 0 );
            vals[1] = vgetq_lane_u64( joined, 1 );
            if ( ( vals[0] | vals[1] ) == 0 )
            {
                continue;
            }
#endif
            for ( uint32_t k = 0; k < 2u; k++ )
            {
                uint64_t top = vals[k];
                while ( top )
                {
                    uint32_t tb = vecsTzcnt( top );
                    uint32_t l2Idx = ( ti + k ) * 64u + tb;
                    uint64_t l2 = q->withMask.l2Masks[l2Idx];
                    
                    for ( uint32_t i = 0; i < q->withoutCount; i++ )
                    {
                        vecsPool* wp = w->pools[q->withoutIds[i]];
                        if ( wp )
                        {
                            l2 &= ~wp->bitfield.l2Masks[l2Idx];
                        }
                    }
                    
                    if ( l2 )
                    {
                        uint32_t lb = vecsTzcnt( l2 );
                        uint32_t entityIdx = l2Idx * 64u + lb;
                        return vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
                    }
                    top &= top - 1;
                }
            }
        }
    }
#endif
    
    for ( ; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = q->withMask.topMasks[ti];
        if ( top == 0 )
        {
            continue;
        }
        
        while ( top )
        {
            uint32_t tb = vecsTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = q->withMask.l2Masks[l2Idx];
            
            for ( uint32_t i = 0; i < q->withoutCount; i++ )
            {
                vecsPool* wp = w->pools[q->withoutIds[i]];
                if ( wp )
                {
                    l2 &= ~wp->bitfield.l2Masks[l2Idx];
                }
            }
            
            if ( l2 )
            {
                uint32_t lb = vecsTzcnt( l2 );
                uint32_t entityIdx = l2Idx * 64u + lb;
                return vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
            }
            top &= top - 1;
        }
    }
    return VECS_INVALID_ENTITY;
}

// Iterates every entity that satisfies the query, invoking fn( entity, With&... ).
//
// The With... types MUST match those used in vecsBuildQuery (or be a subset).
// Passing extra types not present in the query's build list will call getData<>
// on a pool that the query mask never verified, returning garbage pointers.
template< typename... With, typename Fn >
inline void vecsQueryEach( vecsWorld* w, vecsQuery* q, Fn&& fn )
{
    assert( w );
    assert( q );
    static_assert( sizeof...( With ) >= 1, "vecsQueryEach requires at least one With component" );
    vecsQueryRefreshWithMask( w, q );

#ifndef NDEBUG
    assert( ( ( []( vecsQuery* query, uint32_t reqId ) {
        for ( uint32_t i = 0; i < query->withCount; i++ )
        {
            if ( query->withIds[i] == reqId ) return true;
        }
        return false;
    }( q, vecsTypeId<std::remove_cv_t<With>>() ) ) && ... ) && "Query execution requested a component not present in the query's With list!" );
#endif
    
    if ( q->impossible || q->withCount == 0 )
    {
        return;
    }
    
    auto withPools = std::make_tuple( vecsDetail::getPool<With>( w )... );
    bool invalid = false;
    vecsDetail::forEachPool( withPools, [&]( vecsPool* pool )
    {
        if ( !pool || pool->count == 0 )
        {
            invalid = true;
        }
    } );
    if ( invalid )
    {
        return;
    }
    
    auto optionalPools = std::tuple<>();
    auto withoutPools = std::tuple<>();
    
    uint32_t ti = 0;
    vecsSimdLevel simdLevel = vecsRuntimeSimdSupported();
    
#if defined( VECS_AVX2 )
    if ( simdLevel == VECS_SIMD_AVX2 )
    {
        for ( ; ti + 3 < VECS_TOP_COUNT; ti += 4 )
        {
            __m256i joined = _mm256_loadu_si256( ( const __m256i* )&q->withMask.topMasks[ti] );
            if ( _mm256_testz_si256( joined, joined ) )
            {
                continue;
            }
            alignas( 32 ) uint64_t vals[4] = {};
            _mm256_store_si256( ( __m256i* )vals, joined );
            for ( uint32_t k = 0; k < 4u; k++ )
            {
                uint64_t top = vals[k];
                while ( top )
                {
                    uint32_t tb = vecsTzcnt( top );
                    uint32_t l2Idx = ( ti + k ) * 64u + tb;
                    uint64_t l2 = q->withMask.l2Masks[l2Idx];
                    vecsDetail::forEachPool( withPools, [&]( vecsPool* pool ) { if ( pool ) l2 &= pool->bitfield.l2Masks[l2Idx]; } );
                    
                    for ( uint32_t i = 0; i < q->withoutCount; i++ )
                    {
                        vecsPool* wp = w->pools[q->withoutIds[i]];
                        if ( wp )
                        {
                            l2 &= ~wp->bitfield.l2Masks[l2Idx];
                        }
                    }
                    if constexpr ( true )
                    {
                        vecsDetail::forEachPool( withoutPools, [&]( vecsPool* pool )
                        {
                            if ( pool )
                            {
                                l2 &= ~pool->bitfield.l2Masks[l2Idx];
                            }
                        } );
                    }
                    
                    while ( l2 )
                    {
                        uint32_t lb = vecsTzcnt( l2 );
                        uint32_t entityIdx = l2Idx * 64u + lb;
                        
                        bool excluded = false;
                        vecsDetail::forEachPool( withoutPools, [&]( vecsPool* pool )
                        {
                            if ( pool && vecsPoolHas( pool, entityIdx ) )
                            {
                                excluded = true;
                            }
                        } );
                        if ( excluded )
                        {
                            l2 &= l2 - 1;
                            continue;
                        }
                        
                        vecsEntity entity = vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
                        vecsDetail::invokeQueryCallback<With...>( w, withPools, optionalPools, entityIdx, entity, fn, std::make_index_sequence<sizeof...( With )>() );
                        l2 &= l2 - 1;
                    }
                    top &= top - 1;
                }
            }
        }
    }
    else
#endif
#if defined( VECS_SSE2 ) || defined( VECS_NEON )
    if ( simdLevel == VECS_SIMD_SSE2 || simdLevel == VECS_SIMD_NEON )
    {
        for ( ; ti + 1 < VECS_TOP_COUNT; ti += 2 )
        {
            uint64_t vals[2] = {};
#if defined( VECS_SSE2 )
            __m128i joined = _mm_loadu_si128( ( const __m128i* )&q->withMask.topMasks[ti] );
            __m128i zero = _mm_setzero_si128();
            __m128i cmp = _mm_cmpeq_epi32( joined, zero );
            if ( _mm_movemask_epi8( cmp ) == 0xFFFF )
            {
                continue;
            }
            alignas( 16 ) uint64_t alignedVals[2] = {};
            _mm_store_si128( ( __m128i* )alignedVals, joined );
            vals[0] = alignedVals[0];
            vals[1] = alignedVals[1];
#elif defined( VECS_NEON )
            uint64x2_t joined = vld1q_u64( &q->withMask.topMasks[ti] );
            vals[0] = vgetq_lane_u64( joined, 0 );
            vals[1] = vgetq_lane_u64( joined, 1 );
            if ( ( vals[0] | vals[1] ) == 0 )
            {
                continue;
            }
#endif
            for ( uint32_t k = 0; k < 2u; k++ )
            {
                uint64_t top = vals[k];
                while ( top )
                {
                    uint32_t tb = vecsTzcnt( top );
                    uint32_t l2Idx = ( ti + k ) * 64u + tb;
                    uint64_t l2 = q->withMask.l2Masks[l2Idx];
                    vecsDetail::forEachPool( withPools, [&]( vecsPool* pool ) { if ( pool ) l2 &= pool->bitfield.l2Masks[l2Idx]; } );
                    
                    for ( uint32_t i = 0; i < q->withoutCount; i++ )
                    {
                        vecsPool* wp = w->pools[q->withoutIds[i]];
                        if ( wp )
                        {
                            l2 &= ~wp->bitfield.l2Masks[l2Idx];
                        }
                    }
                    if constexpr ( true )
                    {
                        vecsDetail::forEachPool( withoutPools, [&]( vecsPool* pool )
                        {
                            if ( pool )
                            {
                                l2 &= ~pool->bitfield.l2Masks[l2Idx];
                            }
                        } );
                    }
                    
                    while ( l2 )
                    {
                        uint32_t lb = vecsTzcnt( l2 );
                        uint32_t entityIdx = l2Idx * 64u + lb;
                        
                        bool excluded = false;
                        vecsDetail::forEachPool( withoutPools, [&]( vecsPool* pool )
                        {
                            if ( pool && vecsPoolHas( pool, entityIdx ) )
                            {
                                excluded = true;
                            }
                        } );
                        if ( excluded )
                        {
                            l2 &= l2 - 1;
                            continue;
                        }
                        
                        vecsEntity entity = vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
                        vecsDetail::invokeQueryCallback<With...>( w, withPools, optionalPools, entityIdx, entity, fn, std::make_index_sequence<sizeof...( With )>() );
                        l2 &= l2 - 1;
                    }
                    top &= top - 1;
                }
            }
        }
    }
#endif
    
    for ( ; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = q->withMask.topMasks[ti];
        if ( top == 0 )
        {
            continue;
        }

        while ( top )
        {
            uint32_t tb = vecsTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = q->withMask.l2Masks[l2Idx];
            vecsDetail::forEachPool( withPools, [&]( vecsPool* pool ) { if ( pool ) l2 &= pool->bitfield.l2Masks[l2Idx]; } );
            
            for ( uint32_t i = 0; i < q->withoutCount; i++ )
            {
                vecsPool* wp = w->pools[q->withoutIds[i]];
                if ( wp )
                {
                    l2 &= ~wp->bitfield.l2Masks[l2Idx];
                }
            }
            if constexpr ( true )
            {
                vecsDetail::forEachPool( withoutPools, [&]( vecsPool* pool )
                {
                    if ( pool )
                    {
                        l2 &= ~pool->bitfield.l2Masks[l2Idx];
                    }
                } );
            }
            
            while ( l2 )
            {
                uint32_t lb = vecsTzcnt( l2 );
                uint32_t entityIdx = l2Idx * 64u + lb;
                
                bool excluded = false;
                vecsDetail::forEachPool( withoutPools, [&]( vecsPool* pool )
                {
                    if ( pool && vecsPoolHas( pool, entityIdx ) )
                    {
                        excluded = true;
                    }
                } );
                if ( excluded )
                {
                    l2 &= l2 - 1;
                    continue;
                }
                
                vecsEntity entity = vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
                vecsDetail::invokeQueryCallback<With...>( w, withPools, optionalPools, entityIdx, entity, fn, std::make_index_sequence<sizeof...( With )>() );
                l2 &= l2 - 1;
            }
            top &= top - 1;
        }
    }
}

// Thread-safe alternative to vecsQueryEach: materializes every matching entity into
// caller-owned storage as a flat list of vecsQueryHit<With...>, so worker threads can
// iterate the result without touching any shared query state.
//
// vecsQueryEach (and the chunk API) mutate q->withMask on every call, so they cannot be
// driven concurrently from multiple threads against the same query. vecsQueryCollect
// confines that mutation to a single collect pass; the resulting list shares nothing
// with the query.
//
// Contract:
//   * Call this on ONE thread (it refreshes q->withMask). Workers then read the list.
//   * Hits hold POINTERS into the live pools. Any structural edit (adding/removing a
//     component or entity, which can realloc or swap-remove within a pool) invalidates
//     the collected pointers -- re-collect after such edits. Reading/writing existing
//     component values is fine.
//   * Appends; it does NOT clear out. The caller owns clear()/reserve(). Collecting two
//     queries into one buffer is therefore free.
//
// Sink is any type supporting push_back( vecsQueryHit<With...> ) (e.g. std::vector).
// Filtering is the query's job (with/without); value tests ride in the worker loop.
template< typename... With, typename Sink >
inline void vecsQueryCollect( vecsWorld* w, vecsQuery* q, Sink& out )
{
    vecsQueryEach<With...>( w, q, [&out]( vecsEntity entity, With&... components )
    {
        out.push_back( vecsQueryHit<With...>{ entity, std::tuple<With*...>( &components... ) } );
    } );
}

inline uint32_t vecsQueryGetChunks( vecsWorld* w, vecsQuery* q, vecsQueryChunk* outChunks, uint32_t maxChunks )
{
    assert( w );
    assert( q );
    assert( outChunks );
    assert( maxChunks > 0 );
    vecsQueryRefreshWithMask( w, q );

    if ( q->impossible || q->withCount == 0 )
    {
        return 0;
    }

    uint32_t activeTi[VECS_TOP_COUNT];
    uint32_t activeCount = 0;

    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
    {
        if ( q->withMask.topMasks[ti] != 0 )
        {
            activeTi[activeCount++] = ti;
        }
    }

    if ( activeCount == 0 )
    {
        return 0;
    }

    uint32_t jobsToDispatch = ( activeCount < maxChunks ) ? activeCount : maxChunks;
    uint32_t chunksPerJob = activeCount / jobsToDispatch;
    uint32_t remainder = activeCount % jobsToDispatch;

    uint32_t currentIdx = 0;
    for ( uint32_t job = 0; job < jobsToDispatch; job++ )
    {
        uint32_t count = chunksPerJob + ( job < remainder ? 1 : 0 );
        outChunks[job].count = count;
        for ( uint32_t i = 0; i < count; i++ )
        {
            outChunks[job].activeTi[i] = activeTi[currentIdx + i];
        }
        currentIdx += count;
    }

    return jobsToDispatch;
}

// Executes the query over a single chunk (for multi-threaded dispatch via vecsQueryGetChunks).
//
// The With... types MUST match those used in vecsBuildQuery (or be a subset).
// Passing extra types not present in the query's build list will call getData<>
// on a pool that the query mask never verified, returning garbage pointers.
template< typename... With, typename Fn >
inline void vecsQueryExecuteChunk( vecsWorld* w, vecsQuery* q, const vecsQueryChunk* chunk, Fn&& fn )
{
    assert( w );
    assert( q );
    assert( chunk );
    static_assert( sizeof...( With ) >= 1, "vecsQueryExecuteChunk requires at least one With component" );
    vecsQueryRefreshWithMask( w, q );

#ifndef NDEBUG
    assert( ( ( []( vecsQuery* query, uint32_t reqId ) {
        for ( uint32_t i = 0; i < query->withCount; i++ )
        {
            if ( query->withIds[i] == reqId ) return true;
        }
        return false;
    }( q, vecsTypeId<std::remove_cv_t<With>>() ) ) && ... ) && "Query execution requested a component not present in the query's With list!" );
#endif

    if ( q->impossible || q->withCount == 0 || chunk->count == 0 )
    {
        return;
    }

    auto withPools = std::make_tuple( vecsDetail::getPool<With>( w )... );
    bool invalid = false;
    vecsDetail::forEachPool( withPools, [&]( vecsPool* pool )
    {
        if ( !pool || pool->count == 0 )
        {
            invalid = true;
        }
    } );
    if ( invalid )
    {
        return;
    }

    auto optionalPools = std::make_tuple( ( q->optionalCount > 0 ) ? w->pools[q->optionalIds[0]] : nullptr ); 

    vecsSimdLevel simdLevel = vecsRuntimeSimdSupported();
    
#if defined( VECS_AVX2 )
    if ( simdLevel == VECS_SIMD_AVX2 )
    {
        for ( uint32_t i = 0; i < chunk->count; i++ )
        {
            uint32_t ti = chunk->activeTi[i];
            uint64_t top = q->withMask.topMasks[ti];
            if ( top == 0 ) continue;

            while ( top )
            {
                uint32_t tb = vecsTzcnt( top );
                uint32_t l2Idx = ti * 64u + tb;
                uint64_t l2 = q->withMask.l2Masks[l2Idx];
                vecsDetail::forEachPool( withPools, [&]( vecsPool* pool ) { if ( pool ) l2 &= pool->bitfield.l2Masks[l2Idx]; } );
                
                for ( uint32_t k = 0; k < q->withoutCount; k++ )
                {
                    vecsPool* wp = w->pools[q->withoutIds[k]];
                    if ( wp ) l2 &= ~wp->bitfield.l2Masks[l2Idx];
                }

                while ( l2 )
                {
                    uint32_t lb = vecsTzcnt( l2 );
                    uint32_t entityIdx = l2Idx * 64u + lb;
                    vecsEntity entity = vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
                    vecsDetail::invokeQueryCallback<With...>( w, withPools, optionalPools, entityIdx, entity, fn, std::make_index_sequence<sizeof...( With )>() );
                    l2 &= l2 - 1;
                }
                top &= top - 1;
            }
        }
        return;
    }
#endif
#if defined( VECS_SSE2 ) || defined( VECS_NEON )
    if ( simdLevel == VECS_SIMD_SSE2 || simdLevel == VECS_SIMD_NEON )
    {
        for ( uint32_t i = 0; i < chunk->count; i++ )
        {
            uint32_t ti = chunk->activeTi[i];
            uint64_t top = q->withMask.topMasks[ti];
            if ( top == 0 ) continue;

            while ( top )
            {
                uint32_t tb = vecsTzcnt( top );
                uint32_t l2Idx = ti * 64u + tb;
                uint64_t l2 = q->withMask.l2Masks[l2Idx];
                vecsDetail::forEachPool( withPools, [&]( vecsPool* pool ) { if ( pool ) l2 &= pool->bitfield.l2Masks[l2Idx]; } );
                
                for ( uint32_t k = 0; k < q->withoutCount; k++ )
                {
                    vecsPool* wp = w->pools[q->withoutIds[k]];
                    if ( wp ) l2 &= ~wp->bitfield.l2Masks[l2Idx];
                }

                while ( l2 )
                {
                    uint32_t lb = vecsTzcnt( l2 );
                    uint32_t entityIdx = l2Idx * 64u + lb;
                    vecsEntity entity = vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
                    vecsDetail::invokeQueryCallback<With...>( w, withPools, optionalPools, entityIdx, entity, fn, std::make_index_sequence<sizeof...( With )>() );
                    l2 &= l2 - 1;
                }
                top &= top - 1;
            }
        }
        return;
    }
#endif

    for ( uint32_t i = 0; i < chunk->count; i++ )
    {
        uint32_t ti = chunk->activeTi[i];
        uint64_t top = q->withMask.topMasks[ti];
        if ( top == 0 ) continue;

        while ( top )
        {
            uint32_t tb = vecsTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = q->withMask.l2Masks[l2Idx];
            vecsDetail::forEachPool( withPools, [&]( vecsPool* pool ) { if ( pool ) l2 &= pool->bitfield.l2Masks[l2Idx]; } );
            
            for ( uint32_t k = 0; k < q->withoutCount; k++ )
            {
                vecsPool* wp = w->pools[q->withoutIds[k]];
                if ( wp ) l2 &= ~wp->bitfield.l2Masks[l2Idx];
            }

            while ( l2 )
            {
                uint32_t lb = vecsTzcnt( l2 );
                uint32_t entityIdx = l2Idx * 64u + lb;
                vecsEntity entity = vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
                vecsDetail::invokeQueryCallback<With...>( w, withPools, optionalPools, entityIdx, entity, fn, std::make_index_sequence<sizeof...( With )>() );
                l2 &= l2 - 1;
            }
            top &= top - 1;
        }
    }
}

template< typename... With >
inline vecsEntity vecsFirstMatch( vecsWorld* w )
{
    constexpr size_t N = sizeof...( With );
    static_assert( N >= 1, "vecsFirstMatch requires at least one component type" );

    if constexpr ( N == 1 )
    {
        using First = std::tuple_element_t<0, std::tuple<With...>>;
        vecsPool* pool = vecsDetail::getPool<First>( w );
        if ( pool && pool->count > 0 )
        {
            uint32_t entityIdx = pool->denseEntities[0];
            return vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
        }
        return VECS_INVALID_ENTITY;
    }
    else
    {
        auto pools = std::make_tuple( vecsDetail::getPool<With>( w )... );
        bool invalid = false;
        vecsDetail::forEachPool( pools, [&]( vecsPool* pool )
        {
            if ( !pool || pool->count == 0 )
            {
                invalid = true;
            }
        } );
        if ( invalid )
        {
            return VECS_INVALID_ENTITY;
        }

        vecsSimdLevel simdLevel = vecsRuntimeSimdSupported();
#if defined( VECS_SSE2 ) || defined( VECS_NEON ) || defined( VECS_AVX2 )
        if ( simdLevel != VECS_SIMD_SCALAR )
        {
            return vecsDetail::findFirstSimd( w, pools );
        }
#endif
        return vecsDetail::findFirstScalar( w, pools );
    }
}

template< typename... With, typename Fn >
inline void vecsEach( vecsWorld* w, Fn&& fn )
{
    constexpr size_t N = sizeof...( With );
    static_assert( N >= 1, "vecsEach requires at least one component type" );

    if constexpr ( N == 1 )
    {
        using First = std::tuple_element_t<0, std::tuple<With...>>;
        vecsPool* pool = vecsDetail::getPool<First>( w );
        if ( !pool )
        {
            return;
        }
        const uint32_t count = pool->count;
        for ( uint32_t i = 0; i < count; i++ )
        {
            uint32_t entityIdx = pool->denseEntities[i];
            vecsEntity entity = vecsMakeEntity( entityIdx, w->entities->generations[entityIdx] );
            First* data = vecsDetail::getData<First>( pool, entityIdx );
            fn( entity, *data );
        }
    }
    else
    {
        auto pools = std::make_tuple( vecsDetail::getPool<With>( w )... );
        bool invalid = false;
        vecsDetail::forEachPool( pools, [&]( vecsPool* pool )
        {
            if ( !pool || pool->count == 0 )
            {
                invalid = true;
            }
        } );
        if ( invalid )
        {
            return;
        }
        vecsSimdLevel simdLevel = vecsRuntimeSimdSupported();
#if defined( VECS_SSE2 ) || defined( VECS_NEON ) || defined( VECS_AVX2 )
        if ( simdLevel != VECS_SIMD_SCALAR )
        {
            vecsDetail::eachJoinSimd<With...>( w, pools, fn );
            return;
        }
#endif
        vecsDetail::eachJoinScalar<With...>( w, pools, fn );
    }
}

// --------------------------------------------------------------------------
// In-Place Emplacement
// --------------------------------------------------------------------------

template< typename T, typename... Args >
inline T* vecsEmplace( vecsWorld* w, vecsEntity e, Args&&... args )
{
    assert( w );
    assert( vecsAlive( w, e ) );
    uint32_t componentId = vecsTypeId<T>();
    assert( componentId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( componentId ) )
    {
        return nullptr;
    }
    vecsPool* pool = vecsEnsurePool<T>( w );
    if ( !pool )
    {
        return nullptr;
    }
    uint32_t idx = vecsEntityIndex( e );

    if ( vecsPoolHas( pool, idx ) )
    {
        T* ptr = ( T* )vecsPoolGet( pool, idx );
        if constexpr ( !std::is_empty<T>::value )
        {
            if constexpr ( !std::is_trivially_destructible<T>::value )
            {
                ptr->~T();
            }
            new ( ptr ) T( std::forward<Args>( args )... );
        }
        return ptr;
    }

    T* result = nullptr;
    if constexpr ( std::is_empty<T>::value )
    {
        vecsPoolSet( pool, idx, nullptr );
        static T tagValue = {};
        result = &tagValue;
    }
    else
    {
        if ( pool->count == pool->capacity )
        {
            vecsPoolGrow( pool );
        }
        uint32_t denseIdx = pool->count++;
        pool->sparse[idx] = denseIdx;
        pool->denseEntities[denseIdx] = idx;
        result = ( T* )( pool->denseData + ( size_t )denseIdx * pool->stride );
        new ( result ) T( std::forward<Args>( args )... );
        vecsBitfieldSet( &pool->bitfield, idx );
    }

    vecsSetSignatureBit( w, idx, componentId );
    vecsNotifyObservers( w, e, componentId, true, result );
    return result;
}

template< typename T >
inline void vecsAddTag( vecsWorld* w, vecsEntity e )
{
    static_assert( std::is_empty<T>::value, "vecsAddTag can only be used on empty structs (tags)" );
    assert( w );
    assert( vecsAlive( w, e ) );
    uint32_t componentId = vecsTypeId<T>();
    assert( componentId < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( componentId ) )
    {
        return;
    }
    vecsPool* pool = vecsEnsurePool<T>( w );
    if ( !pool )
    {
        return;
    }
    uint32_t idx = vecsEntityIndex( e );

    if ( vecsPoolHas( pool, idx ) )
    {
        return;
    }

    vecsPoolSet( pool, idx, nullptr );
    static T tagValue = {};

    vecsSetSignatureBit( w, idx, componentId );
    vecsNotifyObservers( w, e, componentId, true, &tagValue );
}

// --------------------------------------------------------------------------
// Variadic Helpers
// --------------------------------------------------------------------------

template< typename... T >
inline bool vecsHasAll( vecsWorld* w, vecsEntity e )
{
    return ( vecsHas<T>( w, e ) && ... );
}

template< typename... T >
inline void vecsRemoveAll( vecsWorld* w, vecsEntity e )
{
    ( vecsUnset<T>( w, e ), ... );
}

template< typename... T >
inline bool vecsHasAny( vecsWorld* w, vecsEntity e )
{
    return ( vecsHas<T>( w, e ) || ... );
}

// --------------------------------------------------------------------------
// vecsEach with Optional Entity Parameter
// --------------------------------------------------------------------------

namespace vecsDetail
{

template< typename... With, typename Tuple, typename Fn, size_t... I >
inline void invokeJoinNoEntity( Tuple& pools, uint32_t entityIdx, Fn&& fn, std::index_sequence<I...> )
{
    fn( *getData<With>( std::get<I>( pools ), entityIdx )... );
}

template< typename... With, typename Tuple, typename Fn >
inline void eachJoinScalarNoEntity( vecsWorld* w, Tuple& pools, Fn&& fn )
{
    ( void )w;
    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = UINT64_MAX;
        forEachPool( pools, [&]( vecsPool* pool ) { top &= pool->bitfield.topMasks[ti]; } );
        while ( top )
        {
            uint32_t tb = vecsTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = UINT64_MAX;
            forEachPool( pools, [&]( vecsPool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
            while ( l2 )
            {
                uint32_t lb = vecsTzcnt( l2 );
                uint32_t entityIdx = l2Idx * 64u + lb;
                invokeJoinNoEntity<With...>( pools, entityIdx, fn, std::make_index_sequence<sizeof...( With )>() );
                l2 &= l2 - 1;
            }
            top &= top - 1;
        }
    }
}

template< size_t N >
struct vecsLambdaTraits
{
    static constexpr size_t ArgCount = N;
};

template< typename R, typename C, typename... Args >
vecsLambdaTraits<sizeof...( Args )> vecsDetectLambdaArgs( R ( C::* )( Args... ) const );

template< typename R, typename C, typename... Args >
vecsLambdaTraits<sizeof...( Args )> vecsDetectLambdaArgs( R ( C::* )( Args... ) );

}

template< typename... With, typename Fn >
inline void vecsEachNoEntity( vecsWorld* w, Fn&& fn )
{
    constexpr size_t N = sizeof...( With );
    static_assert( N >= 1, "vecsEachNoEntity requires at least one component type" );
    
    if constexpr ( N == 1 )
    {
        using First = std::tuple_element_t<0, std::tuple<With...>>;
        vecsPool* pool = vecsDetail::getPool<First>( w );
        if ( !pool )
        {
            return;
        }
        for ( uint32_t i = 0; i < pool->count; i++ )
        {
            First* data = vecsDetail::getData<First>( pool, pool->denseEntities[i] );
            fn( *data );
        }
    }
    else
    {
        auto pools = std::make_tuple( vecsDetail::getPool<With>( w )... );
        bool invalid = false;
        vecsDetail::forEachPool( pools, [&]( vecsPool* pool )
        {
            if ( !pool || pool->count == 0 )
            {
                invalid = true;
            }
        } );
        if ( invalid )
        {
            return;
        }
        vecsDetail::eachJoinScalarNoEntity<With...>( w, pools, fn );
    }
}

// --------------------------------------------------------------------------
// Singleton
// --------------------------------------------------------------------------

template< typename T >
inline T* vecsSetSingleton( vecsWorld* w, const T& val = {} )
{
    static_assert( std::is_trivially_copyable<T>::value,
        "vecs singletons must be trivially copyable: snapshots deep-copy via memcpy." );
    assert( w );
    uint32_t id = vecsTypeId<T>();
    assert( id < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( id ) )
    {
        return nullptr;
    }
    auto& slot = w->singletons[id];
    if ( !slot.data )
    {
        slot.data = ( uint8_t* )std::malloc( sizeof( T ) );
        assert( slot.data );
        slot.size = sizeof( T );
        // trivially_copyable ⟹ trivially_destructible (see static_assert above); destructor slot stays null.
        slot.destructor = nullptr;
    }
    else
    {
        assert( slot.size == sizeof( T ) );
        if ( slot.destructor )
        {
            slot.destructor( slot.data );
        }
    }
    new ( slot.data ) T( val );
    return ( T* )slot.data;
}

template< typename T >
inline T* vecsGetSingleton( vecsWorld* w )
{
    assert( w );
    uint32_t id = vecsTypeId<T>();
    assert( id < VECS_MAX_COMPONENTS );
    if ( id >= VECS_MAX_COMPONENTS || !w->singletons[id].data )
    {
        return nullptr;
    }
    return ( T* )w->singletons[id].data;
}

template< typename T >
inline void vecsOnAdd( vecsWorld* w, void ( *callback )( vecsWorld*, vecsEntity, T* ) )
{
    assert( w );
    assert( callback );
    uint32_t id = vecsTypeId<T>();
    assert( id < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( id ) )
    {
        return;
    }
    vecsAddObserver( w->observers, id, reinterpret_cast< void ( * )( vecsWorld*, vecsEntity, void* ) >( callback ), true );
}

template< typename T >
inline void vecsOnRemove( vecsWorld* w, void ( *callback )( vecsWorld*, vecsEntity, T* ) )
{
    assert( w );
    assert( callback );
    uint32_t id = vecsTypeId<T>();
    assert( id < VECS_MAX_COMPONENTS );
    if ( !vecsComponentIdValid( id ) )
    {
        return;
    }
    vecsAddObserver( w->observers, id, reinterpret_cast< void ( * )( vecsWorld*, vecsEntity, void* ) >( callback ), false );
}

inline void vecsSetChildOf( vecsWorld* w, vecsEntity child, vecsEntity parent )
{
    assert( w );
    if ( !vecsAlive( w, child ) ) return;
    if ( parent != VECS_INVALID_ENTITY && !vecsAlive( w, parent ) ) return;
    if ( !w->relationships )
    {
        w->relationships = vecsCreateRelationships( w->maxEntities );
    }
    if ( parent != VECS_INVALID_ENTITY )
    {
        assert( vecsAlive( w, parent ) );
        if ( child == parent )
        {
            return;
        }
        vecsEntity cursor = parent;
        uint32_t guard = 0;
        while ( cursor != VECS_INVALID_ENTITY )
        {
            if ( cursor == child )
            {
                return;
            }
            if ( ++guard > w->maxEntities )
            {
                return;
            }
            cursor = vecsGetParent( w->relationships, cursor );
        }
    }
    vecsSetParent( w->relationships, child, parent );
}

inline vecsEntity vecsGetParentEntity( vecsWorld* w, vecsEntity child )
{
    assert( w );
    assert( vecsAlive( w, child ) );
    if ( !w->relationships )
    {
        return VECS_INVALID_ENTITY;
    }
    return vecsGetParent( w->relationships, child );
}

inline uint32_t vecsGetChildEntityCount( vecsWorld* w, vecsEntity parent )
{
    assert( w );
    assert( vecsAlive( w, parent ) );
    if ( !w->relationships )
    {
        return 0;
    }
    return vecsGetChildCount( w->relationships, parent );
}

inline vecsEntity vecsGetChildEntity( vecsWorld* w, vecsEntity parent, uint32_t index )
{
    assert( w );
    assert( vecsAlive( w, parent ) );
    if ( !w->relationships )
    {
        return VECS_INVALID_ENTITY;
    }
    return vecsGetChild( w->relationships, parent, index );
}

// --------------------------------------------------------------------------
// Entity Handle Wrapper
// --------------------------------------------------------------------------

struct vecsHandle
{
    vecsWorld* w;
    vecsEntity e;
    
    template< typename T, typename... Args >
    T* emplace( Args&&... args )
    {
        return vecsEmplace<T>( w, e, std::forward<Args>( args )... );
    }
    
    template< typename T >
    T* set( const T& val = {} )
    {
        return vecsSet<T>( w, e, val );
    }
    
    template< typename T >
    T* get()
    {
        return vecsGet<T>( w, e );
    }
    
    template< typename T >
    bool has() const
    {
        return vecsHas<T>( w, e );
    }
    
    template< typename T >
    void remove()
    {
        vecsUnset<T>( w, e );
    }
    
    template< typename... T >
    bool hasAll() const
    {
        return vecsHasAll<T...>( w, e );
    }
    
    template< typename... T >
    void removeAll()
    {
        vecsRemoveAll<T...>( w, e );
    }
    
    template< typename T >
    void addTag()
    {
        vecsAddTag<T>( w, e );
    }
    
    void setParent( vecsEntity parent )
    {
        vecsSetChildOf( w, e, parent );
    }
    
    vecsEntity parent() const
    {
        return vecsGetParentEntity( w, e );
    }
    
    uint32_t childCount() const
    {
        return vecsGetChildEntityCount( w, e );
    }
    
    vecsEntity child( uint32_t index ) const
    {
        return vecsGetChildEntity( w, e, index );
    }
    
    bool alive() const
    {
        return vecsAlive( w, e );
    }
    
    void destroy()
    {
        vecsDestroy( w, e );
    }
    
    vecsEntity id() const
    {
        return e;
    }
};

inline vecsHandle vecsCreateHandle( vecsWorld* w )
{
    return { w, vecsCreate( w ) };
}

inline vecsHandle vecsMakeHandle( vecsWorld* w, vecsEntity e )
{
    return { w, e };
}

// --------------------------------------------------------------------------
// World Snapshot - Implementation
// --------------------------------------------------------------------------
//
// Sync: Create -> CaptureInto -> Restore -> Destroy.

// Pool allocator — fast path for std::vector / std::string / similar containers
// inside components. Opt-in: declare components with vecs::pool_allocator<T> instead
// of std::allocator<T>. Allocations route to the active pool (set by detail::PoolScope);
// falls back to malloc when no pool is active. Containers born under an active pool
// must be destroyed under that same active pool; otherwise UB. Default containers
// (pool=nullptr) are malloc-backed and safe to destroy anywhere.
namespace vecs
{
    class BumpPool
    {
        struct Block
        {
            void* base;
            size_t size;
            Block* next;
        };
        Block* head = nullptr;
        // Bytes consumed in head. New allocations land at base + used.
        size_t used = 0;

    public:
        BumpPool() = default;
        BumpPool( const BumpPool& ) = delete;
        BumpPool& operator=( const BumpPool& ) = delete;

        ~BumpPool()
        {
            Block* b = head;
            while ( b )
            {
                Block* n = b->next;
                std::free( b->base );
                std::free( b );
                b = n;
            }
        }

        // Bump pointer never moves already-issued memory: new blocks are
        // pushed onto head and the bump pointer resets to zero there.
        // Allocates with the requested alignment; caller is responsible for
        // not exceeding alignof(max_align_t) at the type level (the std
        // allocator case below handles over-aligned types via posix_memalign).
        void* alloc( size_t bytes, size_t alignment )
        {
            constexpr size_t kBlockBase = 4096;
            size_t aligned = ( used + alignment - 1 ) & ~( alignment - 1 );
            if ( !head || aligned + bytes > head->size )
            {
                size_t ns = std::max( kBlockBase, aligned + bytes + 4096 );
                Block* nb = (Block*)std::malloc( sizeof( Block ) );
                assert( nb );
                nb->base = std::malloc( ns );
                assert( nb->base );
                nb->size = ns;
                nb->next = head;
                head = nb;
                aligned = 0;
                used = bytes;
                return nb->base;
            }
            void* p = (uint8_t*)head->base + aligned;
            used = aligned + bytes;
            return p;
        }

        void reset()
        {
            Block* b = head ? head->next : nullptr;
            while ( b )
            {
                Block* n = b->next;
                std::free( b->base );
                std::free( b );
                b = n;
            }
            if ( head ) head->next = nullptr;
            used = 0;
        }

        bool owns( void* p ) const
        {
            for ( Block* b = head; b; b = b->next )
            {
                uint8_t* lo = (uint8_t*)b->base;
                if ( (uint8_t*)p >= lo && (uint8_t*)p < lo + b->size ) return true;
            }
            return false;
        }
    };

    namespace detail
    {
        // Active pool for the current thread, set by PoolScope.
        inline thread_local BumpPool* tls_active_pool = nullptr;

        struct PoolScope
        {
            BumpPool* prev;
            PoolScope( BumpPool* p ) : prev( tls_active_pool ) { tls_active_pool = p; }
            ~PoolScope() { tls_active_pool = prev; }
        };
    }

    template< typename T >
    class pool_allocator
    {
    public:
        using value_type = T;
        // Two pool_allocator<T> instances are interchangeable iff they refer
        // to the same BumpPool. is_always_equal=false lets containers with
        // different instances know to re-allocate on copy/move. Don't
        // change this to true: it would silently enable buffer-steal moves
        // between containers backed by different pools (live-world leak).
        using is_always_equal = std::false_type;
        using propagate_on_container_copy_assignment = std::false_type;
        using propagate_on_container_move_assignment = std::false_type;
        using propagate_on_container_swap = std::false_type;

        // Default: route to tls_active_pool during PoolScope, else malloc.
        pool_allocator() noexcept = default;

        // Pin to a specific pool (user-owned). All alloc/dealloc go to that pool.
        explicit pool_allocator( BumpPool& p ) noexcept : m_pool( &p ) {}

        // Rebind ctor: copy pool pointer across T types. Uses the public
        // pool_get() accessor so no friend declaration is needed.
        template< typename U >
        pool_allocator( const pool_allocator<U>& o ) noexcept : m_pool( o.pool_get() ) {}

        BumpPool* pool_get() const noexcept { return m_pool; }

        T* allocate( size_t n )
        {
            BumpPool* p = m_pool ? m_pool : detail::tls_active_pool;
            if ( p ) return static_cast<T*>( p->alloc( n * sizeof(T), alignof(T) ) );
            // Fallback: over-aligned types need aligned_alloc; mirror the
            // alignment > 8u branch used elsewhere in this header.
            if constexpr ( alignof(T) > 8u )
            {
                return static_cast<T*>( vecsAlignedAlloc( n * sizeof(T), alignof(T) ) );
            }
            else
            {
                return static_cast<T*>( std::malloc( n * sizeof(T) ) );
            }
        }

        void deallocate( T* p, size_t ) noexcept
        {
            if ( m_pool )
            {
                if ( m_pool->owns( p ) ) return;
                if constexpr ( alignof(T) > 8u ) vecsAlignedFree( p );
                else std::free( p );
                return;
            }
            BumpPool* active = detail::tls_active_pool;
            if ( active && active->owns( p ) ) return;
            if constexpr ( alignof(T) > 8u ) vecsAlignedFree( p );
            else std::free( p );
        }

    private:
        BumpPool* m_pool = nullptr;
    };

    template< typename T, typename U >
    bool operator==( const pool_allocator<T>& a, const pool_allocator<U>& b ) noexcept
    {
        return a.pool_get() == b.pool_get();
    }

    template< typename T, typename U >
    bool operator!=( const pool_allocator<T>& a, const pool_allocator<U>& b ) noexcept
    {
        return a.pool_get() != b.pool_get();
    }
}

namespace vecs_snapshot_detail
{
    struct CapturedPool
    {
        uint32_t      componentId;
        bool          noData;
        uint32_t      stride;
        uint32_t      alignment;
        uint32_t      count;
        uint32_t      bufCapacity;
        uint32_t      hiCapturedSparse;
        vecsBitfield  bitfield;
        uint32_t*     sparse;
        uint32_t*     denseEntities;
        uint8_t*      denseData;
        void ( *destructor )( void* );
        void ( *moveCtor )( void*, void* );
        void ( *copyCtor )( void*, const void* );
    };

    struct CapturedPoolSlot
    {
        CapturedPool pool;
        bool         inUse;
    };

    struct CapturedSingletonSlot
    {
        bool     inUse;
        uint32_t size;
        void   ( *destructor )( void* );
        uint8_t* data;
    };

    struct CapturedSnapshot
    {
        uint32_t* generations;
        uint8_t*  allocated;
        uint32_t* freeList;
        uint64_t* signatures[VECS_SIGNATURE_WORDS];
        uint32_t  freeCount;
        uint32_t  alive;
        uint32_t  maxEntities;
        uint32_t  hiCaptured;

        CapturedPoolSlot*      poolSlots;
        uint32_t               poolCount;
        uint32_t               poolCapacity;

        CapturedSingletonSlot  singletons[VECS_MAX_COMPONENTS];
        bool                   relPresent;
        vecsEntity*            relParents; // [maxEntities]; INVALID_ENTITY means no parent

        // Bump arena for pool_allocator users during capture. Reset each
        // capture so reused buffers go through it. Freed when the snapshot
        // is destroyed.
        vecs::BumpPool         allocatorPool;
    };

    inline CapturedSnapshot* allocCaptured( uint32_t maxEntities )
    {
        CapturedSnapshot* s = ( CapturedSnapshot* )std::malloc( sizeof( CapturedSnapshot ) );
        assert( s );
        std::memset( s, 0, sizeof( CapturedSnapshot ) );
        new ( &s->allocatorPool ) vecs::BumpPool();
        // calloc so rows above hiCaptured read as zero (INVALID_ENTITY for relParents).
        s->generations = ( uint32_t* )std::calloc( maxEntities, sizeof( uint32_t ) );
        s->allocated   = ( uint8_t*  )std::calloc( maxEntities, sizeof( uint8_t ) );
        s->freeList    = ( uint32_t* )std::malloc( maxEntities * sizeof( uint32_t ) );
        s->relParents  = ( vecsEntity* )std::malloc( maxEntities * sizeof( vecsEntity ) );
        assert( s->generations && s->allocated && s->freeList && s->relParents );
        for ( uint32_t i = 0; i < maxEntities; i++ ) s->relParents[i] = VECS_INVALID_ENTITY;
        for ( uint32_t i = 0; i < VECS_SIGNATURE_WORDS; i++ )
        {
            s->signatures[i] = ( uint64_t* )std::calloc( maxEntities, sizeof( uint64_t ) );
            assert( s->signatures[i] );
        }
        s->maxEntities = maxEntities;
        return s;
    }

    inline void freeCaptured( CapturedSnapshot* s )
    {
        if ( !s ) return;
        // Wrap the destructors in PoolScope so vecs::pool_allocator users
        // correctly route dealloc back to the snap's arena (no-op), not
        // to std::free on interior pointers.
        vecs::detail::PoolScope scope( &s->allocatorPool );
        std::free( s->generations );
        std::free( s->allocated );
        std::free( s->freeList );
        for ( uint32_t i = 0; i < VECS_SIGNATURE_WORDS; i++ )
        {
            std::free( s->signatures[i] );
        }
        for ( uint32_t i = 0; i < s->poolCapacity; i++ )
        {
            CapturedPool& p = s->poolSlots[i].pool;
            if ( !s->poolSlots[i].inUse ) continue;
            if ( !p.noData && p.denseData && p.count > 0u )
            {
                if ( p.destructor )
                {
                    for ( uint32_t k = 0; k < p.count; k++ )
                    {
                        p.destructor( p.denseData + ( size_t )k * p.stride );
                    }
                }
            }
            std::free( p.sparse );
            std::free( p.denseEntities );
            if ( p.denseData )
            {
                if ( p.alignment > 8u )
                {
                    vecsAlignedFree( p.denseData );
                }
                else
                {
                    std::free( p.denseData );
                }
            }
        }
        std::free( s->poolSlots );
        for ( uint32_t i = 0; i < VECS_MAX_COMPONENTS; i++ )
        {
            if ( s->singletons[i].inUse )
            {
                if ( s->singletons[i].destructor && s->singletons[i].data )
                {
                    s->singletons[i].destructor( s->singletons[i].data );
                }
                std::free( s->singletons[i].data );
            }
        }
        std::free( s->relParents );
        s->allocatorPool.~BumpPool();
        std::free( s );
    }

    inline CapturedPoolSlot* findOrAddSlot( CapturedSnapshot* s, uint32_t componentId )
    {
        for ( uint32_t i = 0; i < s->poolCount; i++ )
        {
            if ( s->poolSlots[i].inUse && s->poolSlots[i].pool.componentId == componentId )
            {
                return &s->poolSlots[i];
            }
        }
        if ( s->poolCount == s->poolCapacity )
        {
            uint32_t newCap = s->poolCapacity ? s->poolCapacity * 2u : 16u;
            CapturedPoolSlot* ns = ( CapturedPoolSlot* )std::realloc( s->poolSlots, ( size_t )newCap * sizeof( CapturedPoolSlot ) );
            assert( ns );
            std::memset( ns + s->poolCapacity, 0, ( size_t )( newCap - s->poolCapacity ) * sizeof( CapturedPoolSlot ) );
            s->poolSlots = ns;
            s->poolCapacity = newCap;
        }
        CapturedPoolSlot* slot = &s->poolSlots[s->poolCount++];
        std::memset( slot, 0, sizeof( CapturedPoolSlot ) );
        slot->inUse = true;
        slot->pool.componentId = componentId;
        return slot;
    }

    inline void ensureBufCapacity( CapturedPool& p, uint32_t needed, uint32_t maxEntities )
    {
        if ( needed <= p.bufCapacity ) return;
        uint32_t newCap = p.bufCapacity ? p.bufCapacity : 64u;
        while ( newCap < needed ) newCap *= 2u;
        if ( newCap > maxEntities ) newCap = maxEntities;

        uint32_t* newDenseEntities = ( uint32_t* )std::realloc( p.denseEntities, ( size_t )newCap * sizeof( uint32_t ) );
        assert( newDenseEntities );
        p.denseEntities = newDenseEntities;

        if ( !p.noData )
        {
            uint8_t* newDenseData = nullptr;
            if ( p.alignment > 8u )
            {
                newDenseData = ( uint8_t* )vecsAlignedAlloc( ( size_t )newCap * p.stride, p.alignment );
            }
            else
            {
                newDenseData = ( uint8_t* )std::malloc( ( size_t )newCap * p.stride );
            }
            assert( newDenseData );
            if ( p.denseData )
            {
                if ( p.alignment > 8u ) vecsAlignedFree( p.denseData );
                else std::free( p.denseData );
            }
            p.denseData = newDenseData;
        }
        p.bufCapacity = newCap;
    }

    inline void ensureSparseAlloc( CapturedPool& p, uint32_t maxEntities )
    {
        if ( p.sparse ) return;
        // calloc: rows above hiCapturedSparse must read as VECS_INVALID_INDEX.
        p.sparse = ( uint32_t* )std::calloc( maxEntities, sizeof( uint32_t ) );
        assert( p.sparse );
    }

    inline void destructCapturedContents( CapturedPool& p )
    {
        if ( !p.noData && p.destructor && p.denseData && p.count > 0u )
        {
            for ( uint32_t k = 0; k < p.count; k++ )
            {
                p.destructor( p.denseData + ( size_t )k * p.stride );
            }
        }
        p.count = 0;
    }
}

struct vecsWorldSnapshot
{
    vecs_snapshot_detail::CapturedSnapshot* state;
};

inline vecsWorldSnapshot* vecsSnapshotCreate( vecsWorld* w )
{
    assert( w );
    vecsWorldSnapshot* snap = ( vecsWorldSnapshot* )std::malloc( sizeof( vecsWorldSnapshot ) );
    assert( snap );
    snap->state = vecs_snapshot_detail::allocCaptured( w->maxEntities );
    vecsSnapshotCaptureInto( w, snap );
    return snap;
}

inline void vecsSnapshotCaptureInto( vecsWorld* w, vecsWorldSnapshot* snap )
{
    assert( w );
    assert( snap );
    assert( snap->state );
    assert( snap->state->maxEntities == w->maxEntities );

    using namespace vecs_snapshot_detail;
    CapturedSnapshot* s = snap->state;

    // Entity pool: bulk copy raw integers.
    const uint32_t hiAll = w->entities->hiAllocated;
    std::memcpy( s->generations, w->entities->generations, hiAll * sizeof( uint32_t ) );
    std::memcpy( s->allocated,   w->entities->allocated,   hiAll * sizeof( uint8_t ) );
    std::memcpy( s->freeList,    w->entities->freeList,    w->entities->freeCount * sizeof( uint32_t ) );
    s->freeCount = w->entities->freeCount;
    s->alive     = w->entities->alive;
    s->hiCaptured = hiAll;
    for ( uint32_t word = 0; word < VECS_SIGNATURE_WORDS; word++ )
    {
        std::memcpy( s->signatures[word], w->entities->signatures[word], hiAll * sizeof( uint64_t ) );
    }

    // Reset the arena; old captured buffers were owned by it. Components
    // using vecs::pool_allocator route dealloc/alloc through this pool for
    // the duration of the destruct + capture. After the scope exits, the
    // arena can be reset again on the next capture.
    {
        vecs::detail::PoolScope scope( &s->allocatorPool );

    // Destruct any non-trivial contents previously captured (reused buffer).
    for ( uint32_t i = 0; i < s->poolCount; i++ )
    {
        if ( s->poolSlots[i].inUse )
        {
            destructCapturedContents( s->poolSlots[i].pool );
        }
    }
    s->allocatorPool.reset();

    // Walk live pools, capture each.
    for ( uint32_t cid = 0; cid < VECS_MAX_COMPONENTS; cid++ )
    {
        vecsPool* pool = w->pools[cid];
        if ( !pool ) continue;

        CapturedPoolSlot* slot = findOrAddSlot( s, cid );
        CapturedPool& p = slot->pool;

        p.componentId = cid;
        p.noData      = pool->noData;
        p.stride      = pool->stride;
        p.alignment   = pool->alignment;
        p.destructor  = pool->destructor;
        p.moveCtor    = pool->moveCtor;
        p.copyCtor    = pool->copyCtor;
        p.count       = pool->count;

        std::memcpy( &p.bitfield, &pool->bitfield, sizeof( vecsBitfield ) );

        // sparse only allocated for non-empty pools (256KB/pool otherwise)
        if ( pool->count > 0u )
        {
            ensureSparseAlloc( p, w->maxEntities );
            const uint32_t spn = pool->hiSparse;
            std::memcpy( p.sparse, pool->sparse, spn * sizeof( uint32_t ) );
            p.hiCapturedSparse = spn;
        }

        ensureBufCapacity( p, pool->count, w->maxEntities );
        std::memcpy( p.denseEntities, pool->denseEntities, pool->count * sizeof( uint32_t ) );
        if ( !pool->noData && pool->count > 0u )
        {
            if ( pool->copyCtor )
            {
                for ( uint32_t k = 0; k < pool->count; k++ )
                {
                    pool->copyCtor( p.denseData + ( size_t )k * pool->stride,
                                    pool->denseData + ( size_t )k * pool->stride );
                }
            }
            else
            {
                // seqlock read: retry if writer active (gen odd) or gen moved
                for ( uint32_t spin = 0; ; ++spin )
                {
                    uint64_t g1 = std::atomic_load_explicit( &pool->gen, std::memory_order_acquire );
                    if ( g1 & 1ull ) { std::this_thread::yield(); continue; }
                    std::memcpy( p.denseData, pool->denseData, ( size_t )pool->count * pool->stride );
                    std::atomic_thread_fence( std::memory_order_acquire );
                    uint64_t g2 = std::atomic_load_explicit( &pool->gen, std::memory_order_acquire );
                    if ( g1 == g2 ) break;
                    if ( spin > 8 ) std::this_thread::yield();
                }
            }
        }
    }
    } // PoolScope

    // Singletons: deep-copy via memcpy.
    for ( uint32_t cid = 0; cid < VECS_MAX_COMPONENTS; cid++ )
    {
        auto& live = w->singletons[cid];
        auto& snapSlot = s->singletons[cid];
        if ( snapSlot.inUse )
        {
            if ( snapSlot.destructor && snapSlot.data )
            {
                snapSlot.destructor( snapSlot.data );
            }
            std::free( snapSlot.data );
            snapSlot.data = nullptr;
            snapSlot.inUse = false;
            snapSlot.size = 0u;
            snapSlot.destructor = nullptr;
        }
        if ( live.data )
        {
            snapSlot.inUse = true;
            snapSlot.size = live.size;
            snapSlot.destructor = live.destructor;
            snapSlot.data = ( uint8_t* )std::malloc( live.size );
            assert( snapSlot.data );
            std::memcpy( snapSlot.data, live.data, live.size );
        }
    }

    // Relationships: bulk-copy parents[]. children[][] is a derived index rebuilt on restore.
    s->relPresent = ( w->relationships != nullptr );
    if ( w->relationships )
    {
        std::memcpy( s->relParents, w->relationships->parents, hiAll * sizeof( vecsEntity ) );
    }
    else
    {
        for ( uint32_t i = 0; i < w->maxEntities; i++ ) s->relParents[i] = VECS_INVALID_ENTITY;
    }
}

inline void vecsSnapshotDestroy( vecsWorldSnapshot* snap )
{
    if ( !snap ) return;
    vecs_snapshot_detail::freeCaptured( snap->state );
    std::free( snap );
}

inline size_t vecsSnapshotBytes( const vecsWorldSnapshot* snap )
{
    assert( snap );
    assert( snap->state );
    const vecs_snapshot_detail::CapturedSnapshot* s = snap->state;
    size_t bytes = sizeof( vecs_snapshot_detail::CapturedSnapshot );
    bytes += s->maxEntities * ( sizeof( uint32_t ) + sizeof( uint8_t ) + sizeof( uint32_t ) );
    bytes += VECS_SIGNATURE_WORDS * s->maxEntities * sizeof( uint64_t );
    bytes += ( size_t )s->poolCapacity * sizeof( vecs_snapshot_detail::CapturedPoolSlot );
    for ( uint32_t i = 0; i < VECS_MAX_COMPONENTS; i++ )
    {
        if ( s->singletons[i].inUse ) bytes += s->singletons[i].size;
    }
    bytes += s->maxEntities * sizeof( vecsEntity ); // relParents
    for ( uint32_t i = 0; i < s->poolCount; i++ )
    {
        if ( !s->poolSlots[i].inUse ) continue;
        const auto& p = s->poolSlots[i].pool;
        bytes += s->maxEntities * sizeof( uint32_t ); // sparse (when allocated)
        bytes += ( size_t )p.bufCapacity * sizeof( uint32_t );
        if ( !p.noData && p.bufCapacity > 0u )
        {
            bytes += ( size_t )p.bufCapacity * p.stride;
        }
    }
    return bytes;
}

inline void vecsSnapshotRestore( vecsWorld* w, const vecsWorldSnapshot* snap )
{
    assert( w );
    assert( snap );
    assert( snap->state );
    const vecs_snapshot_detail::CapturedSnapshot* s = snap->state;
    assert( s->maxEntities == w->maxEntities && "Snapshot maxEntities must match world" );
    assert( s->maxEntities == w->entities->maxEntities && "Snapshot maxEntities must match entity pool" );

    // Step 1: entity pool bounded by hiCaptured. Zero [n, max(n, w->hiAllocated))
    // so the freeList invariant (no slot both allocated and free) holds even
    // when the world grew past the snapshot's high-water mark.
    const uint32_t n  = s->hiCaptured;
    const uint32_t nClr = ( w->entities->hiAllocated > n ) ? w->entities->hiAllocated : n;
    std::memcpy( w->entities->generations, s->generations, n * sizeof( uint32_t ) );
    if ( nClr > n ) std::memset( w->entities->generations + n, 0, ( nClr - n ) * sizeof( uint32_t ) );
    std::memcpy( w->entities->allocated,   s->allocated,   n * sizeof( uint8_t ) );
    if ( nClr > n ) std::memset( w->entities->allocated + n, 0, ( nClr - n ) * sizeof( uint8_t ) );
    std::memcpy( w->entities->freeList,    s->freeList,    s->freeCount * sizeof( uint32_t ) );
    w->entities->freeCount = s->freeCount;
    w->entities->alive     = s->alive;
    w->entities->hiAllocated = n;
    for ( uint32_t word = 0; word < VECS_SIGNATURE_WORDS; word++ )
    {
        std::memcpy( w->entities->signatures[word], s->signatures[word], n * sizeof( uint64_t ) );
        if ( nClr > n ) std::memset( w->entities->signatures[word] + n, 0, ( nClr - n ) * sizeof( uint64_t ) );
    }

    // Step 2a: pools present-in-world / absent-in-snapshot -> empty (fires onRemove first).
    for ( uint32_t cid = 0; cid < VECS_MAX_COMPONENTS; cid++ )
    {
        vecsPool* pool = w->pools[cid];
        if ( !pool ) continue;
        bool inSnap = false;
        for ( uint32_t i = 0; i < s->poolCount; i++ )
        {
            if ( s->poolSlots[i].inUse && s->poolSlots[i].pool.componentId == cid )
            {
                inSnap = true;
                break;
            }
        }
        if ( inSnap ) continue;

        // Fire onRemove for entities with this component.
        if ( w->observers )
        {
            for ( uint32_t j = 0; j < w->maxEntities; j++ )
            {
                if ( pool->sparse[j] != VECS_INVALID_INDEX )
                {
                    vecsEntity e = vecsMakeEntity( j, w->entities->generations[j] );
                    vecsNotifyObservers( w, e, cid, false, nullptr );
                }
            }
        }
        if ( pool->destructor && !pool->noData )
        {
            for ( uint32_t j = 0; j < pool->count; j++ )
            {
                pool->destructor( pool->denseData + ( size_t )j * pool->stride );
            }
        }
        pool->count = 0;
        vecsBitfieldClearAll( &pool->bitfield );
        for ( uint32_t j = 0; j < w->maxEntities; j++ )
        {
            pool->sparse[j] = VECS_INVALID_INDEX;
        }
    }

    // Step 2b: for each snapshot pool, ensure pool exists and restore contents.
    for ( uint32_t i = 0; i < s->poolCount; i++ )
    {
        if ( !s->poolSlots[i].inUse ) continue;
        const auto& sp = s->poolSlots[i].pool;
        uint32_t cid = sp.componentId;
        vecsPool* pool = w->pools[cid];

        if ( !pool )
        {
            pool = vecsCreatePool( w->maxEntities, sp.stride, sp.alignment,
                                   sp.destructor, sp.moveCtor, sp.copyCtor, sp.noData );
            w->pools[cid] = pool;
        }

        // Destruct current contents before overwriting (preserves capacity).
        if ( pool->destructor && !pool->noData )
        {
            for ( uint32_t j = 0; j < pool->count; j++ )
            {
                pool->destructor( pool->denseData + ( size_t )j * pool->stride );
            }
        }
        pool->count = 0;

        // Grow dense buffers if needed.
        if ( sp.count > pool->capacity )
        {
            uint32_t newCap = sp.count;
            uint32_t* newDenseEntities = ( uint32_t* )std::realloc( pool->denseEntities, ( size_t )newCap * sizeof( uint32_t ) );
            assert( newDenseEntities );
            pool->denseEntities = newDenseEntities;
            if ( !sp.noData )
            {
                uint8_t* newDenseData = nullptr;
                if ( sp.alignment > 8u )
                {
                    newDenseData = ( uint8_t* )vecsAlignedAlloc( ( size_t )newCap * sp.stride, sp.alignment );
                }
                else
                {
                    newDenseData = ( uint8_t* )std::malloc( ( size_t )newCap * sp.stride );
                }
                assert( newDenseData );
                if ( pool->denseData )
                {
                    if ( pool->alignment > 8u ) vecsAlignedFree( pool->denseData );
                    else std::free( pool->denseData );
                }
                pool->denseData = newDenseData;
            }
            pool->capacity = newCap;
            // pool->alignment already matches snapshot (identical type)
        }

        // Bulk-restore contents.
        // count==0 may still hold stale sparse from prior capture; don't copy it
        if ( sp.count > 0u && sp.sparse && pool->sparse )
        {
            // Bounded by sp.hiCapturedSparse; clear up to the live range.
            const uint32_t spn = sp.hiCapturedSparse;
            std::memcpy( pool->sparse, sp.sparse, spn * sizeof( uint32_t ) );
            const uint32_t spClr = ( pool->hiSparse > spn ) ? pool->hiSparse : spn;
            if ( spClr > spn ) std::memset( pool->sparse + spn, 0xFFu, ( spClr - spn ) * sizeof( uint32_t ) );
            pool->hiSparse = spn;
        }
        else
        {
            for ( uint32_t j = 0; j < w->maxEntities; j++ )
            {
                pool->sparse[j] = VECS_INVALID_INDEX;
            }
        }
        std::memcpy( &pool->bitfield, &sp.bitfield, sizeof( vecsBitfield ) );
        std::memcpy( pool->denseEntities, sp.denseEntities, sp.count * sizeof( uint32_t ) );
        if ( !sp.noData && sp.count > 0u )
        {
            if ( sp.copyCtor )
            {
                for ( uint32_t k = 0; k < sp.count; k++ )
                {
                    sp.copyCtor( pool->denseData + ( size_t )k * sp.stride,
                                 sp.denseData + ( size_t )k * sp.stride );
                }
            }
            else
            {
                std::memcpy( pool->denseData, sp.denseData, ( size_t )sp.count * sp.stride );
            }
        }
        pool->count = sp.count;
        // stride/alignment/noData/fn-ptrs unchanged (identical type -> identical)

        // Fire onAdd for restored entities.
        if ( w->observers )
        {
            for ( uint32_t k = 0; k < sp.count; k++ )
            {
                uint32_t idx = pool->denseEntities[k];
                if ( !pool->noData )
                {
                    void* data = pool->denseData + ( size_t )k * pool->stride;
                    vecsEntity e = vecsMakeEntity( idx, w->entities->generations[idx] );
                    vecsNotifyObservers( w, e, cid, true, data );
                }
            }
        }
    }

    // Step 3: singletons.
    for ( uint32_t cid = 0; cid < VECS_MAX_COMPONENTS; cid++ )
    {
        auto& live = w->singletons[cid];
        auto& slot = s->singletons[cid];
        if ( slot.inUse )
        {
            if ( !live.data )
            {
                live.data = ( uint8_t* )std::malloc( slot.size );
                assert( live.data );
                live.size = slot.size;
                live.destructor = slot.destructor;
            }
            else
            {
                assert( live.size == slot.size );
                if ( live.destructor ) live.destructor( live.data );
            }
            std::memcpy( live.data, slot.data, slot.size );
        }
        else if ( live.data )
        {
            if ( live.destructor ) live.destructor( live.data );
            std::free( live.data );
            live.data = nullptr;
            live.size = 0u;
            live.destructor = nullptr;
        }
    }

    // Step 4: relationships. Bulk-copy parents[] bounded by hiCaptured;
    // rebuild children[][] from snap data also bounded to live range.
    if ( !w->relationships )
    {
        w->relationships = vecsCreateRelationships( w->maxEntities );
    }
    // Free stale child arrays above hiCaptured (world may have grown since snap).
    std::memcpy( w->relationships->parents, s->relParents, n * sizeof( vecsEntity ) );
    for ( uint32_t i = 0; i < nClr; i++ )
    {
        w->relationships->childCounts[i] = 0u;
        if ( w->relationships->children[i] )
        {
            std::free( w->relationships->children[i] );
            w->relationships->children[i] = nullptr;
        }
        w->relationships->childCapacities[i] = 0u;
    }
    // Rows above nClr stay as-is; they were zero in a fresh world and the
    // children-free loop only touches the snapped range.
    for ( uint32_t i = 0; i < n; i++ )
    {
        vecsEntity parent = s->relParents[i];
        if ( parent == VECS_INVALID_ENTITY ) continue;
        uint32_t parentIdx = vecsEntityIndex( parent );
        if ( parentIdx >= w->maxEntities ) continue;
        w->relationships->childCounts[parentIdx]++;
    }
    for ( uint32_t p = 0; p < n; p++ )
    {
        uint32_t cnt = w->relationships->childCounts[p];
        if ( cnt == 0u ) continue;
        w->relationships->children[p] = ( vecsEntity* )std::malloc( cnt * sizeof( vecsEntity ) );
        w->relationships->childCapacities[p] = cnt;
    }
    uint32_t* cursor = ( uint32_t* )std::calloc( w->maxEntities, sizeof( uint32_t ) );
    for ( uint32_t i = 0; i < n; i++ )
    {
        vecsEntity parent = s->relParents[i];
        if ( parent == VECS_INVALID_ENTITY ) continue;
        uint32_t parentIdx = vecsEntityIndex( parent );
        if ( parentIdx >= w->maxEntities ) continue;
        uint32_t slot = cursor[parentIdx]++;
        w->relationships->children[parentIdx][slot] = vecsMakeEntity( i, w->entities->generations[i] );
    }
    std::free( cursor );
}


// Opaque, owns its own storage. Deep-copies components via each pool's
// copyCtor when set; memcpy fallback for trivially-copyable types.
struct vecsWorldSnapshot;

// Blocking capture from the calling thread. world must be quiescent
// (no concurrent mutations). For thread-safe capture see vecsSnapshotCreate.
vecsWorldSnapshot* vecsSnapshotCreate( vecsWorld* w );

// Hot-path capture: reuse an existing snapshot's buffers. Grows only,
// never shrinks; high-water mark is maintained per pool.
void vecsSnapshotCaptureInto( vecsWorld* w, vecsWorldSnapshot* snap );

// Restore world to the snapshot state.
//  - entity pool (generations/allocated/freeList/signatures/freeCount/alive)
//    is bulk-overwritten so entity indices + generations match exactly.
//  - pools present in snapshot but absent from world are recreated.
//  - pools absent from snapshot but present in world are emptied.
//  - dense ordering and free-list ordering are preserved exactly.
//  - observer callbacks are NOT fired.
void vecsSnapshotRestore( vecsWorld* w, const vecsWorldSnapshot* snap );

void vecsSnapshotDestroy( vecsWorldSnapshot* snap );

size_t vecsSnapshotBytes( const vecsWorldSnapshot* snap );

// --------------------------------------------------------------------------
// SIMD
// --------------------------------------------------------------------------

