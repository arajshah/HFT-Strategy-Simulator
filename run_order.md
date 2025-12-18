# Run Order (repro checklist)

## 1) Build (Release)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## 2) Generate deterministic tape
```bash
rm -f out/tape_gen.csv
./build/hft_sim --tape out/tape_gen.csv --scenario gen_v1 --seed 123 --n 200000 --overwrite
```

## 3) Replay determinism check (single-thread)
```bash
./build/hft_sim --tape out/tape_gen.csv
```

## 4) Replay determinism check (pipeline)
```bash
./build/hft_sim --tape out/tape_gen.csv --pipeline
```

## 5) Benchmark (pipeline, 5 iters)
```bash
./build/hft_sim --tape out/tape_gen.csv --bench --pipeline --iters 5
```

## 6) Strategy run (baseline)
```bash
./build/hft_sim --tape out/tape_gen.csv \
  --strategy periodic_taker \
  --strategy_every 1000 \
  --strategy_qty 10 \
  --strategy_side alternate
```

## 7) Debug invariants sanity (small n)
```bash
cmake -S . -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug -j
rm -f out/tape_dbg.csv
./build_debug/hft_sim --tape out/tape_dbg.csv --scenario gen_v1 --seed 123 --n 10000 --overwrite --pipeline
```

