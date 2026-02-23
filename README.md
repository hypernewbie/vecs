# Vecs

Vecs is a single-header, header-only C++ ECS using a two-level bitfield sparse index for fast
`has()` checks and efficient multi-component joins.

## Quick Start

```cpp
#include "vecs.h"

struct Position { float x, y; };
struct Velocity { float vx, vy; };

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
