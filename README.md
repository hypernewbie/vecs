# Vecs

![CI](https://github.com/hypernewbie/vecs/actions/workflows/ci.yml/badge.svg)

Vecs is a high-performance, single-header C++ ECS utilising a **two-level bitfield sparse index** for O(1) lookups and efficient vectorized joins.

The architecture is inspired by the design described by [Sebastien Aaltonen](https://x.com/SebAaltonen/status/2018706822429892859).

> Vecs is AI coded slop, without much oversight. Not closely engineering reviewed.
> Don't use this library. Please write something better instead.

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

## Performance

Benchmarks performed on **Windows (AMD Ryzen 9 5950X)** comparing **Vecs (Auto SIMD)** vs **EnTT (v3.13.0)**.

### Core Operations

| Operation | Vecs | EnTT | Ratio |
| :--- | :--- | :--- | :--- |
| **Entity Create** | 1.65 B ops/s | 68.50 M ops/s | **24.0x** |
| **Component Insert** | 154.46 M ops/s | 44.24 M ops/s | **3.4x** |
| **Query Iterate** | 72.32 B ops/s | 936.24 M ops/s | **77.2x** |
| **Full Destroy** | 37.77 M ops/s | 11.45 M ops/s | **3.2x** |
| **Shotgun Deletion** | 34.94 M ops/s | 14.33 M ops/s | **2.4x** |

There are also a lot of projects out there that use EnTT as a basis for comparison (this should already tell you a lot). This is one of those projects. Our benchmarks are completely wrong and incomplete, good at omitting basically all useful information, and we used the wrong function to compare several features. However, even if used poorly, no one cares because no one uses Vecs.

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

## Debugger Visualizations

Vecs includes a `.natvis` file for enhanced debugging in Visual Studio. The visualizers provide human‑readable views of entity IDs, component pools, relationships, queries, and command buffers.

**Installation**:
1. Copy `vecs.natvis` to your solution directory.
2. In Visual Studio: **Debug → Options → Debugging → Symbols → Add .natvis file**.
3. Or add to your `.vcxproj`:
   ```xml
   <ItemGroup>
     <Natvis Include="vecs.natvis" />
   </ItemGroup>
   ```

**Features**:
- **vecsEntity**: Shows `index:generation` instead of raw 64‑bit value.
- **vecsWorld**: Displays alive entity count, active component pools, relationships, observers, and singletons.
- **vecsPool**: Shows component count, stride, and bitfield state.
- **vecsQuery**: Lists required, excluded, and optional component IDs.
- **vecsCommandBuffer**: Lists deferred commands and pending entity creations.

Use `,view(simple)` in the Watch window to reduce detail.

## Notes

- C++17 minimum.
- Keep all ECS code in `vecs.h`.
- Tests live in one file: `vecs_test.cpp`.
