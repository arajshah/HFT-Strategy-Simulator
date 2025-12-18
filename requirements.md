# Requirements / Versions

## Toolchain
- CMake 3.20+ recommended
- A C++20-capable compiler
  - Apple Clang (Xcode command line tools) works on macOS
  - GCC 11+ / Clang 14+ should also work

## Runtime
- No external runtime dependencies.
- Outputs are CSV tapes written to `out/`.

## Notes
- Benchmark timing varies by machine and build type. Use `Release` for performance numbers.
- Debug builds enable assertions for order book invariants and can be much slower.

