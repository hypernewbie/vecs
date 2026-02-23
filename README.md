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
    veWorld* w = veCreateWorld();
    veEntity e = veCreate( w );

    veSet<Position>( w, e, { 0.0f, 0.0f } );
    veSet<Velocity>( w, e, { 1.0f, 0.5f } );

    veEach<Position, Velocity>( w, []( veEntity, Position& p, Velocity& v )
    {
        p.x += v.vx;
        p.y += v.vy;
    } );

    veDestroyWorld( w );
    return 0;
}
```

## API Reference

### Entity Creation & Destruction

```cpp
veEntity e = veCreate( w );           // Create entity
veDestroy( w, e );                    // Destroy entity (recursively destroys children)
bool alive = veAlive( w, e );         // Check if entity is alive
uint32_t count = veCount( w );        // Get alive entity count
```

### Component Operations

```cpp
// Set component (copy-based)
veSet<Position>( w, e, { x, y } );

// Emplace component (in-place construction, no copy)
veEmplace<Position>( w, e, x, y );    // Constructs Position(x, y) directly in pool
veEmplace<std::string>( w, e, "hello", 5 );  // Works with complex types

// Get component pointer
Position* p = veGet<Position>( w, e );

// Check if entity has component
bool has = veHas<Position>( w, e );

// Remove component
veUnset<Position>( w, e );
```

### Zero-Byte Tags

```cpp
struct IsEnemy {};  // Empty struct = tag

veAddTag<IsEnemy>( w, e );   // Clean API for tags
bool isEnemy = veHas<IsEnemy>( w, e );
```

### Variadic Helpers

```cpp
// Check if entity has ALL components
if ( veHasAll<Position, Velocity, Health>( w, e ) ) { ... }

// Check if entity has ANY component
if ( veHasAny<Position, Velocity>( w, e ) ) { ... }

// Remove multiple components at once
veRemoveAll<Position, Velocity, Health>( w, e );
```

### Entity Handle (Ergonomic C++ Wrapper)

```cpp
// Create entity with handle wrapper
veHandle player = veCreateHandle( w );
player.emplace<Position>( 10.0f, 20.0f );
player.emplace<Velocity>( 1.0f, 0.0f );
player.addTag<IsEnemy>();

// Access components
Position* p = player.get<Position>();
if ( player.hasAll<Position, Velocity>() ) { ... }

// Parent/child relationships
player.setParent( parentEntity );
veEntity parent = player.parent();

// Cleanup
player.destroy();
```

### Iteration

```cpp
// Full callback with entity ID
veEach<Position, Velocity>( w, []( veEntity e, Position& p, Velocity& v )
{
    p.x += v.vx;
} );

// Simplified callback (no entity ID)
veEachNoEntity<Position>( w, []( Position& p )
{
    p.y -= 9.8f;  // Just mutate data, don't care about entity
} );
```

### Relationships (Parent/Child)

```cpp
veSetChildOf( w, child, parent );            // Set parent
veEntity parent = veGetParentEntity( w, child );
uint32_t n = veGetChildEntityCount( w, parent );
veEntity c = veGetChildEntity( w, parent, 0 );

// veDestroy recursively destroys all children
veDestroy( w, parent );  // Destroys parent + all descendants
```

### Observers (Reactive Events)

```cpp
void onPositionAdded( veWorld* w, veEntity e, Position* p ) { ... }

veOnAdd<Position>( w, onPositionAdded );
veOnRemove<Position>( w, onPositionRemoved );
```

### Command Buffers (Deferred Operations)

```cpp
veCommandBuffer* cb = veCreateCommandBuffer( w );

uint32_t idx = veCmdCreate( cb );
veCmdSetCreated<Position>( cb, idx, { 1, 2 } );
veCmdDestroy( cb, someEntity );
veCmdSetParent( cb, child, parent );

veFlush( cb );  // Execute all commands
veEntity e = veCmdGetCreated( cb, idx );

veDestroyCommandBuffer( cb );
```

### Cached Queries

```cpp
veQuery* q = veBuildQuery<Position, Velocity>( w );
veQueryAddWithout( q, veTypeId<Dead>() );  // Exclude entities with Dead tag

veQueryEach<Position, Velocity>( w, q, []( veEntity, Position& p, Velocity& v )
{
    p.x += v.vx;
} );

veDestroyQuery( q );
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
