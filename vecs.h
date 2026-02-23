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
#include <type_traits>
#include <tuple>
#include <utility>

#ifndef VECS_NO_SIMD
    #if defined( __SSE2__ ) || defined( _M_X64 ) || defined( _M_AMD64 )
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

struct vePool
{
    veBitfield bitfield;
    uint32_t* sparse;
    uint32_t* denseEntities;
    uint8_t* denseData;
    uint32_t count;
    uint32_t capacity;
    uint32_t stride;
    void ( *destructor )( void* );
};

inline void vePoolGrow( vePool* pool )
{
    assert( pool );
    uint32_t newCapacity = pool->capacity ? pool->capacity * 2u : 64u;
    if ( newCapacity > VECS_MAX_ENTITIES )
    {
        newCapacity = VECS_MAX_ENTITIES;
    }
    assert( newCapacity > pool->capacity );
    uint32_t* newDenseEntities = ( uint32_t* )std::realloc( pool->denseEntities, newCapacity * sizeof( uint32_t ) );
    uint8_t* newDenseData = ( uint8_t* )std::realloc( pool->denseData, ( size_t )newCapacity * pool->stride );
    assert( newDenseEntities );
    assert( newDenseData );
    pool->denseEntities = newDenseEntities;
    pool->denseData = newDenseData;
    pool->capacity = newCapacity;
}

inline vePool* veCreatePool( uint32_t maxEntities, uint32_t stride, void ( *dtor )( void* ) = nullptr )
{
    vePool* pool = ( vePool* )std::malloc( sizeof( vePool ) );
    assert( pool );
    pool->sparse = ( uint32_t* )std::malloc( maxEntities * sizeof( uint32_t ) );
    pool->denseEntities = ( uint32_t* )std::malloc( 64u * sizeof( uint32_t ) );
    pool->denseData = ( uint8_t* )std::malloc( ( size_t )64u * stride );
    assert( pool->sparse );
    assert( pool->denseEntities );
    assert( pool->denseData );
    for ( uint32_t i = 0; i < maxEntities; i++ )
    {
        pool->sparse[i] = VECS_INVALID_INDEX;
    }
    veBitfieldClearAll( &pool->bitfield );
    pool->count = 0;
    pool->capacity = 64u;
    pool->stride = stride;
    pool->destructor = dtor;
    return pool;
}

inline void veDestroyPool( vePool* pool )
{
    if ( !pool )
    {
        return;
    }
    if ( pool->destructor )
    {
        for ( uint32_t i = 0; i < pool->count; i++ )
        {
            pool->destructor( pool->denseData + ( size_t )i * pool->stride );
        }
    }
    std::free( pool->sparse );
    std::free( pool->denseEntities );
    std::free( pool->denseData );
    std::free( pool );
}

inline void* vePoolSet( vePool* pool, uint32_t entityIndex, const void* data )
{
    assert( pool );
    assert( entityIndex < VECS_MAX_ENTITIES );
    assert( !veBitfieldHas( &pool->bitfield, entityIndex ) );
    if ( pool->count == pool->capacity )
    {
        vePoolGrow( pool );
    }
    uint32_t denseIdx = pool->count++;
    pool->sparse[entityIndex] = denseIdx;
    pool->denseEntities[denseIdx] = entityIndex;
    uint8_t* dst = pool->denseData + ( size_t )denseIdx * pool->stride;
    std::memcpy( dst, data, pool->stride );
    veBitfieldSet( &pool->bitfield, entityIndex );
    return dst;
}

inline void vePoolUnset( vePool* pool, uint32_t entityIndex )
{
    assert( pool );
    assert( entityIndex < VECS_MAX_ENTITIES );
    assert( veBitfieldHas( &pool->bitfield, entityIndex ) );
    uint32_t denseIdx = pool->sparse[entityIndex];
    uint32_t lastIdx = pool->count - 1u;
    uint8_t* removePtr = pool->denseData + ( size_t )denseIdx * pool->stride;
    if ( pool->destructor )
    {
        pool->destructor( removePtr );
    }
    if ( denseIdx != lastIdx )
    {
        uint8_t* lastPtr = pool->denseData + ( size_t )lastIdx * pool->stride;
        std::memcpy( removePtr, lastPtr, pool->stride );
        uint32_t movedEntity = pool->denseEntities[lastIdx];
        pool->denseEntities[denseIdx] = movedEntity;
        pool->sparse[movedEntity] = denseIdx;
    }
    veBitfieldUnset( &pool->bitfield, entityIndex );
    pool->sparse[entityIndex] = VECS_INVALID_INDEX;
    pool->count--;
}

inline void* vePoolGet( vePool* pool, uint32_t entityIndex )
{
    assert( pool );
    if ( !veBitfieldHas( &pool->bitfield, entityIndex ) )
    {
        return nullptr;
    }
    uint32_t denseIdx = pool->sparse[entityIndex];
    return pool->denseData + ( size_t )denseIdx * pool->stride;
}

inline bool vePoolHas( const vePool* pool, uint32_t entityIndex )
{
    assert( pool );
    return veBitfieldHas( &pool->bitfield, entityIndex );
}

// --------------------------------------------------------------------------
// World
// --------------------------------------------------------------------------

struct veWorld
{
    struct veSingletonSlot
    {
        uint8_t* data;
        uint32_t size;
        void ( *destructor )( void* );
    };

    veEntityPool* entities;
    vePool* pools[VECS_MAX_COMPONENTS];
    veSingletonSlot singletons[VECS_MAX_COMPONENTS];
    uint32_t maxEntities;
};

inline uint32_t veNextTypeId()
{
    static uint32_t counter = 0;
    return counter++;
}

template< typename T >
inline uint32_t veTypeId()
{
    static uint32_t id = veNextTypeId();
    return id;
}

inline veWorld* veCreateWorld( uint32_t maxEntities = VECS_MAX_ENTITIES )
{
    veWorld* world = ( veWorld* )std::malloc( sizeof( veWorld ) );
    assert( world );
    world->entities = veCreateEntityPool( maxEntities );
    world->maxEntities = maxEntities;
    std::memset( world->pools, 0, sizeof( world->pools ) );
    std::memset( world->singletons, 0, sizeof( world->singletons ) );
    return world;
}

inline void veDestroyWorld( veWorld* world )
{
    if ( !world )
    {
        return;
    }
    for ( uint32_t i = 0; i < VECS_MAX_COMPONENTS; i++ )
    {
        veDestroyPool( world->pools[i] );
        if ( world->singletons[i].data )
        {
            if ( world->singletons[i].destructor )
            {
                world->singletons[i].destructor( world->singletons[i].data );
            }
            std::free( world->singletons[i].data );
        }
    }
    veDestroyEntityPool( world->entities );
    std::free( world );
}

inline veEntity veCreate( veWorld* w )
{
    assert( w );
    return veEntityPoolCreate( w->entities );
}

inline void veDestroy( veWorld* w, veEntity e )
{
    assert( w );
    assert( veEntityPoolAlive( w->entities, e ) );
    uint32_t entityIndex = veEntityIndex( e );
    for ( uint32_t i = 0; i < VECS_MAX_COMPONENTS; i++ )
    {
        vePool* pool = w->pools[i];
        if ( pool && vePoolHas( pool, entityIndex ) )
        {
            vePoolUnset( pool, entityIndex );
        }
    }
    veEntityPoolDestroy( w->entities, e );
}

inline bool veAlive( veWorld* w, veEntity e )
{
    assert( w );
    return veEntityPoolAlive( w->entities, e );
}

inline uint32_t veCount( veWorld* w )
{
    assert( w );
    return w->entities->alive;
}

template< typename T >
inline vePool* veEnsurePool( veWorld* w )
{
    uint32_t id = veTypeId<T>();
    assert( id < VECS_MAX_COMPONENTS );
    if ( !w->pools[id] )
    {
        void ( *dtor )( void* ) = nullptr;
        if constexpr ( !std::is_trivially_destructible<T>::value )
        {
            dtor = []( void* ptr ) { static_cast<T*>( ptr )->~T(); };
        }
        w->pools[id] = veCreatePool( w->maxEntities, sizeof( T ), dtor );
    }
    return w->pools[id];
}

template< typename T >
inline T* veSet( veWorld* w, veEntity e, const T& val = {} )
{
    assert( veAlive( w, e ) );
    vePool* pool = veEnsurePool<T>( w );
    uint32_t idx = veEntityIndex( e );
    if ( vePoolHas( pool, idx ) )
    {
        T* ptr = ( T* )vePoolGet( pool, idx );
        *ptr = val;
        return ptr;
    }
    return ( T* )vePoolSet( pool, idx, &val );
}

template< typename T >
inline void veUnset( veWorld* w, veEntity e )
{
    assert( veAlive( w, e ) );
    vePool* pool = veEnsurePool<T>( w );
    uint32_t idx = veEntityIndex( e );
    assert( vePoolHas( pool, idx ) );
    vePoolUnset( pool, idx );
}

template< typename T >
inline T* veGet( veWorld* w, veEntity e )
{
    assert( veAlive( w, e ) );
    uint32_t id = veTypeId<T>();
    if ( id >= VECS_MAX_COMPONENTS || !w->pools[id] )
    {
        return nullptr;
    }
    return ( T* )vePoolGet( w->pools[id], veEntityIndex( e ) );
}

template< typename T >
inline bool veHas( veWorld* w, veEntity e )
{
    uint32_t id = veTypeId<T>();
    if ( id >= VECS_MAX_COMPONENTS || !w->pools[id] )
    {
        return false;
    }
    return vePoolHas( w->pools[id], veEntityIndex( e ) );
}

inline veEntity veClone( veWorld* w, veEntity src )
{
    assert( veAlive( w, src ) );
    veEntity dst = veCreate( w );
    if ( dst == VECS_INVALID_ENTITY )
    {
        return VECS_INVALID_ENTITY;
    }

    uint32_t srcIdx = veEntityIndex( src );
    uint32_t dstIdx = veEntityIndex( dst );
    for ( uint32_t i = 0; i < VECS_MAX_COMPONENTS; i++ )
    {
        vePool* pool = w->pools[i];
        if ( pool && vePoolHas( pool, srcIdx ) )
        {
            void* srcData = vePoolGet( pool, srcIdx );
            vePoolSet( pool, dstIdx, srcData );
        }
    }
    return dst;
}

// --------------------------------------------------------------------------
// Query
// --------------------------------------------------------------------------

namespace veDetail
{
template< typename T >
inline vePool* getPool( veWorld* w )
{
    uint32_t id = veTypeId<T>();
    if ( id >= VECS_MAX_COMPONENTS )
    {
        return nullptr;
    }
    return w->pools[id];
}

template< typename T >
inline T* getData( vePool* pool, uint32_t entityIdx )
{
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
inline void invokeJoin( veWorld* w, Tuple& pools, uint32_t entityIdx, Fn&& fn, std::index_sequence<I...> )
{
    veEntity entity = veMakeEntity( entityIdx, w->entities->generations[entityIdx] );
    fn( entity, *getData<Components>( std::get<I>( pools ), entityIdx )... );
}

template< typename... Components, typename Tuple, typename Fn >
inline void eachJoinScalar( veWorld* w, Tuple& pools, Fn&& fn )
{
    constexpr size_t N = sizeof...( Components );
    for ( uint32_t ti = 0; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = UINT64_MAX;
        forEachPool( pools, [&]( vePool* pool ) { top &= pool->bitfield.topMasks[ti]; } );
        while ( top )
        {
            uint32_t tb = veTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = UINT64_MAX;
            forEachPool( pools, [&]( vePool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
            while ( l2 )
            {
                uint32_t lb = veTzcnt( l2 );
                uint32_t entityIdx = l2Idx * 64u + lb;
                invokeJoin<Components...>( w, pools, entityIdx, fn, std::make_index_sequence<N>() );
                l2 &= l2 - 1;
            }
            top &= top - 1;
        }
    }
}

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
template< typename Tuple, size_t... I >
inline uint64x2_t andTopMasksNeon( Tuple& pools, uint32_t ti, std::index_sequence<I...> )
{
    uint64x2_t joined = vdupq_n_u64( UINT64_MAX );
    ( ( joined = vandq_u64( joined, vld1q_u64( &std::get<I>( pools )->bitfield.topMasks[ti] ) ) ), ... );
    return joined;
}
#endif

template< typename... Components, typename Tuple, typename Fn >
inline void eachJoinSimd( veWorld* w, Tuple& pools, Fn&& fn )
{
    constexpr size_t N = sizeof...( Components );
    constexpr size_t PoolCount = std::tuple_size<Tuple>::value;
    uint32_t ti = 0;
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
        // TODO: test this on NEON hardware since SIMD is not validated in this environment.
        uint64x2_t cmp = vceqq_u64( joined, vdupq_n_u64( 0 ) );
        if ( vgetq_lane_u64( cmp, 0 ) && vgetq_lane_u64( cmp, 1 ) )
        {
            continue;
        }
        vals[0] = vgetq_lane_u64( joined, 0 );
        vals[1] = vgetq_lane_u64( joined, 1 );
#endif
        for ( uint32_t k = 0; k < 2u; k++ )
        {
            uint64_t top = vals[k];
            while ( top )
            {
                uint32_t tb = veTzcnt( top );
                uint32_t l2Idx = ( ti + k ) * 64u + tb;
                uint64_t l2 = UINT64_MAX;
                forEachPool( pools, [&]( vePool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
                while ( l2 )
                {
                    uint32_t lb = veTzcnt( l2 );
                    uint32_t entityIdx = l2Idx * 64u + lb;
                    invokeJoin<Components...>( w, pools, entityIdx, fn, std::make_index_sequence<N>() );
                    l2 &= l2 - 1;
                }
                top &= top - 1;
            }
        }
    }
    for ( ; ti < VECS_TOP_COUNT; ti++ )
    {
        uint64_t top = UINT64_MAX;
        forEachPool( pools, [&]( vePool* pool ) { top &= pool->bitfield.topMasks[ti]; } );
        while ( top )
        {
            uint32_t tb = veTzcnt( top );
            uint32_t l2Idx = ti * 64u + tb;
            uint64_t l2 = UINT64_MAX;
            forEachPool( pools, [&]( vePool* pool ) { l2 &= pool->bitfield.l2Masks[l2Idx]; } );
            while ( l2 )
            {
                uint32_t lb = veTzcnt( l2 );
                uint32_t entityIdx = l2Idx * 64u + lb;
                invokeJoin<Components...>( w, pools, entityIdx, fn, std::make_index_sequence<N>() );
                l2 &= l2 - 1;
            }
            top &= top - 1;
        }
    }
}
}

inline bool veRuntimeSimdSupported()
{
#if defined( VECS_SSE2 )
    static int cached = -1;
    if ( cached < 0 )
    {
        #if defined( _M_X64 ) || defined( _M_AMD64 )
            cached = 1;
        #elif defined( _MSC_VER ) || defined( _WIN32 )
            int cpuInfo[4] = {};
            __cpuid( cpuInfo, 1 );
            cached = ( cpuInfo[3] & ( 1 << 26 ) ) ? 1 : 0;
        #elif defined( __x86_64__ ) || defined( __i386__ )
            cached = __builtin_cpu_supports( "sse2" ) ? 1 : 0;
        #else
            cached = 0;
        #endif
    }
    return cached == 1;
#elif defined( VECS_NEON )
    return true;
#else
    return false;
#endif
}

template< typename... Components, typename Fn >
inline void veEach( veWorld* w, Fn&& fn )
{
    constexpr size_t N = sizeof...( Components );
    static_assert( N >= 1, "veEach requires at least one component type" );

    if constexpr ( N == 1 )
    {
        using First = std::tuple_element_t<0, std::tuple<Components...>>;
        vePool* pool = veDetail::getPool<First>( w );
        if ( !pool )
        {
            return;
        }
        for ( uint32_t i = 0; i < pool->count; i++ )
        {
            uint32_t entityIdx = pool->denseEntities[i];
            veEntity entity = veMakeEntity( entityIdx, w->entities->generations[entityIdx] );
            First* data = ( First* )( pool->denseData + ( size_t )i * pool->stride );
            fn( entity, *data );
        }
    }
    else
    {
        auto pools = std::make_tuple( veDetail::getPool<Components>( w )... );
        bool invalid = false;
        veDetail::forEachPool( pools, [&]( vePool* pool )
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
#if defined( VECS_SSE2 ) || defined( VECS_NEON )
        if ( veRuntimeSimdSupported() )
        {
            veDetail::eachJoinSimd<Components...>( w, pools, fn );
            return;
        }
#endif
        veDetail::eachJoinScalar<Components...>( w, pools, fn );
    }
}

// --------------------------------------------------------------------------
// Singleton
// --------------------------------------------------------------------------

template< typename T >
inline T* veSetSingleton( veWorld* w, const T& val = {} )
{
    uint32_t id = veTypeId<T>();
    assert( id < VECS_MAX_COMPONENTS );
    auto& slot = w->singletons[id];
    if ( !slot.data )
    {
        slot.data = ( uint8_t* )std::malloc( sizeof( T ) );
        assert( slot.data );
        slot.size = sizeof( T );
        slot.destructor = nullptr;
        if constexpr ( !std::is_trivially_destructible<T>::value )
        {
            slot.destructor = []( void* ptr ) { static_cast<T*>( ptr )->~T(); };
        }
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
inline T* veGetSingleton( veWorld* w )
{
    uint32_t id = veTypeId<T>();
    if ( id >= VECS_MAX_COMPONENTS || !w->singletons[id].data )
    {
        return nullptr;
    }
    return ( T* )w->singletons[id].data;
}

// --------------------------------------------------------------------------
// Command Buffer
// --------------------------------------------------------------------------

struct veCommand
{
    enum Type : uint8_t
    {
        CREATE,
        DESTROY,
        SET_COMPONENT,
        UNSET_COMPONENT
    };

    Type type;
    veEntity entity;
    uint32_t componentId;
    uint32_t dataOffset;
    uint32_t dataSize;
    uint32_t createdIndex;
    vePool* pool;
};

struct veCommandBuffer
{
    veWorld* world;
    veCommand* commands;
    uint32_t commandCount;
    uint32_t commandCapacity;
    uint8_t* dataBuffer;
    uint32_t dataSize;
    uint32_t dataCapacity;
    veEntity* created;
    uint32_t createdCount;
    uint32_t createdCapacity;
};

inline void veCmdGrowCommands( veCommandBuffer* cb )
{
    uint32_t cap = cb->commandCapacity ? cb->commandCapacity * 2u : 64u;
    veCommand* ptr = ( veCommand* )std::realloc( cb->commands, ( size_t )cap * sizeof( veCommand ) );
    assert( ptr );
    cb->commands = ptr;
    cb->commandCapacity = cap;
}

inline void veCmdGrowData( veCommandBuffer* cb, uint32_t minExtra )
{
    uint32_t need = cb->dataSize + minExtra;
    if ( cb->dataCapacity >= need )
    {
        return;
    }
    uint32_t cap = cb->dataCapacity ? cb->dataCapacity * 2u : 256u;
    while ( cap < need )
    {
        cap *= 2u;
    }
    uint8_t* ptr = ( uint8_t* )std::realloc( cb->dataBuffer, cap );
    assert( ptr );
    cb->dataBuffer = ptr;
    cb->dataCapacity = cap;
}

inline void veCmdGrowCreated( veCommandBuffer* cb )
{
    uint32_t cap = cb->createdCapacity ? cb->createdCapacity * 2u : 32u;
    veEntity* ptr = ( veEntity* )std::realloc( cb->created, ( size_t )cap * sizeof( veEntity ) );
    assert( ptr );
    cb->created = ptr;
    cb->createdCapacity = cap;
}

inline veCommandBuffer* veCreateCommandBuffer( veWorld* w )
{
    veCommandBuffer* cb = ( veCommandBuffer* )std::malloc( sizeof( veCommandBuffer ) );
    assert( cb );
    cb->world = w;
    cb->commands = nullptr;
    cb->commandCount = 0;
    cb->commandCapacity = 0;
    cb->dataBuffer = nullptr;
    cb->dataSize = 0;
    cb->dataCapacity = 0;
    cb->created = nullptr;
    cb->createdCount = 0;
    cb->createdCapacity = 0;
    return cb;
}

inline void veDestroyCommandBuffer( veCommandBuffer* cb )
{
    if ( !cb )
    {
        return;
    }
    std::free( cb->commands );
    std::free( cb->dataBuffer );
    std::free( cb->created );
    std::free( cb );
}

inline uint32_t veCmdCreate( veCommandBuffer* cb )
{
    assert( cb );
    if ( cb->createdCount == cb->createdCapacity )
    {
        veCmdGrowCreated( cb );
    }
    uint32_t index = cb->createdCount++;
    cb->created[index] = VECS_INVALID_ENTITY;

    if ( cb->commandCount == cb->commandCapacity )
    {
        veCmdGrowCommands( cb );
    }
    veCommand& cmd = cb->commands[cb->commandCount++];
    cmd.type = veCommand::CREATE;
    cmd.entity = VECS_INVALID_ENTITY;
    cmd.componentId = VECS_INVALID_INDEX;
    cmd.dataOffset = 0;
    cmd.dataSize = 0;
    cmd.createdIndex = index;
    cmd.pool = nullptr;
    return index;
}

inline void veCmdDestroy( veCommandBuffer* cb, veEntity e )
{
    assert( cb );
    if ( cb->commandCount == cb->commandCapacity )
    {
        veCmdGrowCommands( cb );
    }
    veCommand& cmd = cb->commands[cb->commandCount++];
    cmd.type = veCommand::DESTROY;
    cmd.entity = e;
    cmd.componentId = VECS_INVALID_INDEX;
    cmd.dataOffset = 0;
    cmd.dataSize = 0;
    cmd.createdIndex = VECS_INVALID_INDEX;
    cmd.pool = nullptr;
}

template< typename T >
inline void veCmdSet( veCommandBuffer* cb, veEntity e, const T& val )
{
    assert( cb );
    vePool* pool = veEnsurePool<T>( cb->world );
    if ( cb->commandCount == cb->commandCapacity )
    {
        veCmdGrowCommands( cb );
    }
    veCmdGrowData( cb, sizeof( T ) );
    veCommand& cmd = cb->commands[cb->commandCount++];
    cmd.type = veCommand::SET_COMPONENT;
    cmd.entity = e;
    cmd.componentId = veTypeId<T>();
    cmd.dataOffset = cb->dataSize;
    cmd.dataSize = sizeof( T );
    cmd.createdIndex = VECS_INVALID_INDEX;
    cmd.pool = pool;
    std::memcpy( cb->dataBuffer + cb->dataSize, &val, sizeof( T ) );
    cb->dataSize += sizeof( T );
}

template< typename T >
inline void veCmdUnset( veCommandBuffer* cb, veEntity e )
{
    assert( cb );
    if ( cb->commandCount == cb->commandCapacity )
    {
        veCmdGrowCommands( cb );
    }
    veCommand& cmd = cb->commands[cb->commandCount++];
    cmd.type = veCommand::UNSET_COMPONENT;
    cmd.entity = e;
    cmd.componentId = veTypeId<T>();
    cmd.dataOffset = 0;
    cmd.dataSize = 0;
    cmd.createdIndex = VECS_INVALID_INDEX;
    cmd.pool = veDetail::getPool<T>( cb->world );
}

inline void veFlush( veCommandBuffer* cb )
{
    assert( cb );
    for ( uint32_t i = 0; i < cb->commandCount; i++ )
    {
        const veCommand& cmd = cb->commands[i];
        switch ( cmd.type )
        {
            case veCommand::CREATE:
                cb->created[cmd.createdIndex] = veCreate( cb->world );
                break;
            case veCommand::DESTROY:
                if ( veAlive( cb->world, cmd.entity ) )
                {
                    veDestroy( cb->world, cmd.entity );
                }
                break;
            case veCommand::SET_COMPONENT:
                if ( veAlive( cb->world, cmd.entity ) )
                {
                    uint32_t idx = veEntityIndex( cmd.entity );
                    void* data = cb->dataBuffer + cmd.dataOffset;
                    if ( vePoolHas( cmd.pool, idx ) )
                    {
                        std::memcpy( vePoolGet( cmd.pool, idx ), data, cmd.dataSize );
                    }
                    else
                    {
                        vePoolSet( cmd.pool, idx, data );
                    }
                }
                break;
            case veCommand::UNSET_COMPONENT:
                if ( veAlive( cb->world, cmd.entity ) && cmd.pool )
                {
                    uint32_t idx = veEntityIndex( cmd.entity );
                    if ( vePoolHas( cmd.pool, idx ) )
                    {
                        vePoolUnset( cmd.pool, idx );
                    }
                }
                break;
            default:
                assert( false );
                break;
        }
    }
    cb->commandCount = 0;
    cb->dataSize = 0;
}

inline veEntity veCmdGetCreated( veCommandBuffer* cb, uint32_t index )
{
    assert( cb );
    assert( index < cb->createdCount );
    return cb->created[index];
}

inline void veCreateBatch( veWorld* w, veEntity* out, uint32_t count )
{
    assert( w );
    assert( out );
    for ( uint32_t i = 0; i < count; i++ )
    {
        out[i] = veCreate( w );
    }
}

// --------------------------------------------------------------------------
// SIMD
// --------------------------------------------------------------------------
