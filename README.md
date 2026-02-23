# Vecs

Vecs is a single-header, header-only C++ ECS using a two-level bitfield sparse index for fast
`has()` checks and efficient multi-component joins.

## Quick Start

```cpp
#include "vecs.h"

struct Position { float x, y; };
struct Velocity { float vx, vy; };
struct IsEnemy {};  // Zero-byte tag

int main()
{
    vecsWorld* w = vecsCreateWorld();
    vecsEntity e = vecsCreate( w );

    vecsSet<Position>( w, e, { 0.0f, 0.0f } );
    vecsSet<Velocity>( w, e, { 1.0f, 0.5f } );

    vecsEach<Position, Velocity>( w, []( vecsEntity, Position& p, Velocity& v )
    {
        p.x += v.vx;
        p.y += v.vy;
    } );

    vecsDestroyWorld( w );
    return 0;
}
```

## API Reference

### Entity Creation & Destruction

```cpp
vecsEntity e = vecsCreate( w );           // Create entity
vecsDestroy( w, e );                    // Destroy entity (recursively destroys children)
bool alive = vecsAlive( w, e );         // Check if entity is alive
uint32_t count = vecsCount( w );        // Get alive entity count
```

### Component Operations

```cpp
// Set component (copy-based)
vecsSet<Position>( w, e, { x, y } );

// Emplace component (in-place construction, no copy)
vecsEmplace<Position>( w, e, x, y );    // Constructs Position(x, y) directly in pool
vecsEmplace<std::string>( w, e, "hello", 5 );  // Works with complex types

// Get component pointer
Position* p = vecsGet<Position>( w, e );

// Check if entity has component
bool has = vecsHas<Position>( w, e );

// Remove component
vecsUnset<Position>( w, e );
```

### Zero-Byte Tags

```cpp
struct IsEnemy {};  // Empty struct = tag

vecsAddTag<IsEnemy>( w, e );   // Clean API for tags
bool isEnemy = vecsHas<IsEnemy>( w, e );
```

### Variadic Helpers

```cpp
// Check if entity has ALL components
if ( vecsHasAll<Position, Velocity, Health>( w, e ) ) { ... }

// Check if entity has ANY component
if ( vecsHasAny<Position, Velocity>( w, e ) ) { ... }

// Remove multiple components at once
vecsRemoveAll<Position, Velocity, Health>( w, e );
```

### Entity Handle (Ergonomic C++ Wrapper)

```cpp
// Create entity with handle wrapper
vecsHandle player = vecsCreateHandle( w );
player.emplace<Position>( 10.0f, 20.0f );
player.emplace<Velocity>( 1.0f, 0.0f );
player.addTag<IsEnemy>();

// Access components
Position* p = player.get<Position>();
if ( player.hasAll<Position, Velocity>() ) { ... }

// Parent/child relationships
player.setParent( parentEntity );
vecsEntity parent = player.parent();

// Cleanup
player.destroy();
```

### Iteration

```cpp
// Full callback with entity ID
vecsEach<Position, Velocity>( w, []( vecsEntity e, Position& p, Velocity& v )
{
    p.x += v.vx;
} );

// Simplified callback (no entity ID)
vecsEachNoEntity<Position>( w, []( Position& p )
{
    p.y -= 9.8f;  // Just mutate data, don't care about entity
} );
```

### Relationships (Parent/Child)

```cpp
vecsSetChildOf( w, child, parent );            // Set parent
vecsEntity parent = vecsGetParentEntity( w, child );
uint32_t n = vecsGetChildEntityCount( w, parent );
vecsEntity c = vecsGetChildEntity( w, parent, 0 );

// vecsDestroy recursively destroys all children
vecsDestroy( w, parent );  // Destroys parent + all descendants
```

### Observers (Reactive Events)

```cpp
void onPositionAdded( vecsWorld* w, vecsEntity e, Position* p ) { ... }

vecsOnAdd<Position>( w, onPositionAdded );
vecsOnRemove<Position>( w, onPositionRemoved );
```

### Command Buffers (Deferred Operations)

```cpp
vecsCommandBuffer* cb = vecsCreateCommandBuffer( w );

uint32_t idx = vecsCmdCreate( cb );
vecsCmdSetCreated<Position>( cb, idx, { 1, 2 } );
vecsCmdDestroy( cb, someEntity );
vecsCmdSetParent( cb, child, parent );

vecsFlush( cb );  // Execute all commands
vecsEntity e = vecsCmdGetCreated( cb, idx );

vecsDestroyCommandBuffer( cb );
```

### Cached Queries

```cpp
vecsQuery* q = vecsBuildQuery<Position, Velocity>( w );
vecsQueryAddWithout( q, vecsTypeId<Dead>() );  // Exclude entities with Dead tag

vecsQueryEach<Position, Velocity>( w, q, []( vecsEntity, Position& p, Velocity& v )
{
    p.x += v.vx;
} );

vecsDestroyQuery( q );
```

## Build

### Windows (clang-cl)
```bash
cmake -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -B build .
cmake --build build
.\build\vecs_test.exe
```

### Linux / macOS (clang)
```bash
cmake -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -B build .
cmake --build build
./build/vecs_test
```

## Notes

- C++17 minimum.
- Keep all ECS code in `vecs.h`.
- Tests live in one file: `vecs_test.cpp`.
