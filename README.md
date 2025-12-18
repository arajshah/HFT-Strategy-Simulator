# High-Frequency Trading Strategy Simulator (HFT Strategy Simulator)

A deterministic, event-driven limit order book (LOB) simulator for experimenting with order flow, replay, performance benchmarking, and strategy evaluation.

## What this repo provides

- Deterministic event tape format (CSV) + deterministic replay verification.
- Price-level LOB with FIFO priority at each price.
- Event-driven matching (adds, cancels, partial fills).
- Producer/consumer pipeline mode with lock-free SPSC ring buffer.
- Benchmark mode with multi-iteration stats + optional breakdown.
- Strategy interface + baseline strategy runner producing PnL/position.

## Build

### Release
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Debug (assertions enabled)
```bash
cmake -S . -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug -j
```

## Core concepts

### Tape

A tape is a deterministic CSV of market events (AddLimit and Cancel), written once and then replayed.

### Replay guarantee

Replay runs the same tape twice (fresh engine/book each time) and checks that the fill signature matches exactly.

## Usage

1) Create a tape (deterministic generator)
```bash
rm -f out/tape_gen.csv
./build/hft_sim --tape out/tape_gen.csv --scenario gen_v1 --seed 123 --n 200000 --overwrite
```

2) Replay a tape (determinism check)
```bash
./build/hft_sim --tape out/tape_gen.csv
```
You should see REPLAY_OK.

3) Replay via pipeline (producer/consumer + lock-free SPSC)
```bash
./build/hft_sim --tape out/tape_gen.csv --pipeline
```

4) Benchmark (single run)
```bash
./build/hft_sim --tape out/tape_gen.csv --bench --pipeline
```

5) Benchmark (repeat + median stats)
```bash
./build/hft_sim --tape out/tape_gen.csv --bench --pipeline --iters 5
```

6) Benchmark (repeat + breakdown per iteration)
```bash
./build/hft_sim --tape out/tape_gen.csv --bench --pipeline --iters 5 --breakdown
```

7) Run the baseline strategy on a tape (PnL/position)
```bash
./build/hft_sim --tape out/tape_gen.csv \
  --strategy periodic_taker \
  --strategy_every 1000 \
  --strategy_qty 10 \
  --strategy_side alternate
```

## CLI reference (high level)

- `--tape <path>`: tape path (read if exists, write if missing or `--overwrite`)
- `--overwrite`: overwrite tape if it exists
- `--scenario fifo_demo|gen_v1`: choose scenario (default fifo_demo)
- `--seed <u64>`: generator seed (gen_v1)
- `--n <size>`: generator event count (gen_v1)
- `--pipeline`: run replay/bench using pipeline mode (producer/consumer)
- `--bench`: benchmark mode (exits after printing metrics)
- `--iters <k>`: number of benchmark iterations (default 1)
- `--breakdown`: print per-iter timing breakdown lines
- `--strategy periodic_taker|none`: run strategy mode (exits after printing results)
- `--strategy_every <k>`: act every k events (periodic_taker)
- `--strategy_qty <q>`: order quantity (periodic_taker)
- `--strategy_side buy|sell|alternate`: side mode (periodic_taker)

## Notes on determinism

- Event ordering is deterministic: sorted by (ts, seq) before processing.
- Matching uses price-time priority (FIFO inside each price level).
- Pipeline mode preserves determinism by sorting before enqueueing.

## Project layout

- `include/hft/` — core headers (order book, engine, generator, pipeline, bench, strategies)
- `src/` — implementations
- `apps/sim_main.cpp` — CLI entrypoint
- `out/` — generated tapes (created at runtime)