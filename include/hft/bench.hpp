// Benchmark harness: runs the engine over generated events and reports timing and counts.
#pragma once

#include "hft/event.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace hft {

struct BenchResult {
  std::size_t n_events{0};
  std::size_t n_add{0};
  std::size_t n_cancel{0};

  std::size_t n_fills{0};
  std::int64_t traded_qty{0};

  double seconds{0.0};
  double events_per_sec{0.0};
  double fills_per_sec{0.0};
  double ns_per_event{0.0};

  // Optional breakdown (seconds)
  double sort_seconds{0.0};
  double run_seconds{0.0};
};

// Bench runs a single engine run over the provided events and reports timing + counts.
// Note: timing is machine-dependent and not expected to be bit-identical across machines.
BenchResult bench_run(std::vector<Event> events);

// Same metrics, but executes via producer/consumer pipeline (Phase 7).
BenchResult bench_run_pipeline(std::vector<Event> events);

struct BenchSummary {
  std::size_t iters{0};
  std::size_t n_events{0};

  // events/sec stats across iters
  double eps_min{0.0};
  double eps_median{0.0};
  double eps_max{0.0};

  // ns/event stats across iters
  double ns_min{0.0};
  double ns_median{0.0};
  double ns_max{0.0};
};

// Runs iters benchmarks. If pipeline=true uses pipeline path; otherwise single-thread engine.
// If breakdown=true, includes sort_seconds/run_seconds per-iter (reported in per-iter lines).
BenchSummary bench_repeat(std::vector<Event> events,
                          bool pipeline,
                          std::size_t iters,
                          bool breakdown);

}  // namespace hft

