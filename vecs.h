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
    veEntityPool* entities;
    vePool* pools[VECS_MAX_COMPONENTS];
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
