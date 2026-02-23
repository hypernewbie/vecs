# Vecs

Vecs is a single-header, header-only C++ ECS using a two-level bitfield sparse index for fast
`has()` checks and efficient multi-component joins.

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
