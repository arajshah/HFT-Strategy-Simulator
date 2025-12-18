# High Frequency Trading Strategy Simulator (HFT Strategy Simulator)

Phase 0 delivers a buildable C++20 skeleton with a minimal OrderBook that supports:

- add limit orders
- cancels
- simple matching + partial fills
- top-of-book snapshot

## Build & Run (macOS)

From repo root:

```bash
cmake -S . -B build
cmake --build build -j
./build/hft_sim
```

Expected output includes a TOP line, at least one FILL line, then a CANCEL line.

