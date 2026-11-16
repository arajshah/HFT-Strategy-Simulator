// sim_main: command-line entry point for the HFT strategy simulator.
//
// Loads a tape of events from CSV, optionally generates one, and runs
// the matching engine twice to verify deterministic replay.

#include "hft/bench.hpp"
#include "hft/engine.hpp"
#include "hft/generator.hpp"
#include "hft/pipeline.hpp"
#include "hft/strategy.hpp"
#include "hft/strategy_runner.hpp"
#include "hft/tape.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

static std::vector<hft::Event> default_scenario() {
  std::vector<hft::Event> events;
  std::uint64_t seq = 0;

  // Two asks at the SAME price (101). FIFO should fill maker=1 before maker=2.
  events.push_back(hft::Event::add_limit(1, ++seq, hft::Order{.id = 1, .side = hft::Side::Sell, .price_ticks = 101, .qty = 20}));
  events.push_back(hft::Event::add_limit(1, ++seq, hft::Order{.id = 2, .side = hft::Side::Sell, .price_ticks = 101, .qty = 20}));

  // Another ask above, to keep book non-trivial
  events.push_back(hft::Event::add_limit(1, ++seq, hft::Order{.id = 6, .side = hft::Side::Sell, .price_ticks = 102, .qty = 50}));

  // Bids
  events.push_back(hft::Event::add_limit(1, ++seq, hft::Order{.id = 3, .side = hft::Side::Buy, .price_ticks =  99, .qty = 40}));
  events.push_back(hft::Event::add_limit(1, ++seq, hft::Order{.id = 4, .side = hft::Side::Buy, .price_ticks = 100, .qty = 40}));

  // Aggressive buy crosses price 101 for qty=30:
  // expected fills: 5:1@101x20 then 5:2@101x10 (FIFO at 101)
  events.push_back(hft::Event::add_limit(2, ++seq, hft::Order{.id = 5, .side = hft::Side::Buy, .price_ticks = 101, .qty = 30}));

  // Cancel a still-resting order (id=6 at 102)
  events.push_back(hft::Event::cancel(3, ++seq, 6));

  return events;
}

static std::string get_tape_path(int argc, char** argv) {
  // Usage: ./hft_sim --tape out/tape.csv
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--tape") {
      return std::string(argv[i + 1]);
    }
  }
  return "out/tape.csv";
}

static std::string get_str_arg(int argc, char** argv, const std::string& key, const std::string& def) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == key) return std::string(argv[i + 1]);
  }
  return def;
}

static std::uint64_t get_u64_arg(int argc, char** argv, const std::string& key, std::uint64_t def) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == key) {
      try {
        return static_cast<std::uint64_t>(std::stoull(argv[i + 1]));
      } catch (...) {
        return def;
      }
    }
  }
  return def;
}

static std::size_t get_size_arg(int argc, char** argv, const std::string& key, std::size_t def) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == key) {
      try {
        return static_cast<std::size_t>(std::stoull(argv[i + 1]));
      } catch (...) {
        return def;
      }
    }
  }
  return def;
}

static bool has_flag(int argc, char** argv, const std::string& key) {
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == key) return true;
  }
  return false;
}

int main(int argc, char** argv) {
  const std::string tape_path = get_tape_path(argc, argv);

  std::cout << "TAPE_PATH " << tape_path << "\n";

  const std::string scenario = get_str_arg(argc, argv, "--scenario", "fifo_demo");
  const std::uint64_t seed = get_u64_arg(argc, argv, "--seed", 1ULL);
  const std::size_t n = get_size_arg(argc, argv, "--n", 1000);
  const bool overwrite = has_flag(argc, argv, "--overwrite");

  std::cout << "SCENARIO " << scenario << "\n";
  std::cout << "SEED " << seed << "\n";
  std::cout << "N_EVENTS " << n << "\n";
  std::cout << "OVERWRITE " << (overwrite ? "true" : "false") << "\n";

  const bool exists = std::filesystem::exists(tape_path);
  if (!exists || overwrite) {
    std::vector<hft::Event> events;

    if (scenario == "fifo_demo") {
      events = default_scenario();
    } else if (scenario == "gen_v1") {
      events = hft::generate_events_gen_v1(seed, n, /*mid_px=*/101, /*spread_ticks=*/5);
    } else {
      std::cout << "BAD_SCENARIO (use fifo_demo or gen_v1)\n";
      return 1;
    }

    std::string err;
    if (!hft::write_events_csv(tape_path, events, &err)) {
      std::cout << "TAPE_WRITE_ERR " << err << "\n";
      return 1;
    }
    std::cout << "TAPE_WRITTEN\n";
  } else {
    std::cout << "TAPE_EXISTS\n";
  }

  std::string read_err;
  const auto loaded = hft::read_events_csv(tape_path, &read_err);
  if (loaded.empty()) {
    std::cout << "TAPE_READ_ERR " << read_err << "\n";
    return 1;
  }
  std::cout << "TAPE_LOADED_OK events=" << loaded.size() << "\n";
  const bool bench = has_flag(argc, argv, "--bench");
  std::cout << "BENCH_MODE " << (bench ? "true" : "false") << "\n";
  const bool pipeline = has_flag(argc, argv, "--pipeline");
  std::cout << "PIPELINE_MODE " << (pipeline ? "true" : "false") << "\n";

  const std::string strat_name = get_str_arg(argc, argv, "--strategy", "none");
  if (strat_name != "none") {
    // Strategy mode ignores --bench/--pipeline for now (single-thread deterministic runner).
    const std::size_t every = get_size_arg(argc, argv, "--strategy_every", 1000);
    const std::uint64_t qty_u = get_u64_arg(argc, argv, "--strategy_qty", 10);
    const std::string side_mode = get_str_arg(argc, argv, "--strategy_side", "alternate");

    std::cout << "STRATEGY " << strat_name << "\n";
    std::cout << "STRATEGY_EVERY " << every << "\n";
    std::cout << "STRATEGY_QTY " << qty_u << "\n";
    std::cout << "STRATEGY_SIDE " << side_mode << "\n";

    if (strat_name != "periodic_taker") {
      std::cout << "BAD_STRATEGY (use periodic_taker or none)\n";
      return 1;
    }

    hft::PeriodicTakerStrategy strat(every, static_cast<std::int64_t>(qty_u), side_mode);
    const auto sr = hft::run_strategy(loaded, strat);

    std::cout << "STRAT_NAME " << sr.strategy_name << "\n";
    std::cout << "STRAT_ACTIONS " << sr.n_actions << "\n";
    std::cout << "STRAT_FILLS " << sr.n_fills << "\n";
    std::cout << "STRAT_TRADED_QTY " << sr.traded_qty << "\n";
    std::cout << "STRAT_POS " << sr.final_position << "\n";
    std::cout << "STRAT_MID " << sr.final_mid << "\n";
    std::cout << "STRAT_CASH " << sr.cash << "\n";
    std::cout << "STRAT_PNL_MTM " << sr.pnl_mtm << "\n";
    std::cout << "STRAT_FINAL_TOP " << sr.final_top << "\n";
    return 0;
  }

  if (bench) {
    const std::size_t iters = get_size_arg(argc, argv, "--iters", 1);
    const bool breakdown = has_flag(argc, argv, "--breakdown");
    std::cout << "BENCH_ITERS " << iters << "\n";
    std::cout << "BENCH_BREAKDOWN " << (breakdown ? "true" : "false") << "\n";

    const auto summary = hft::bench_repeat(loaded, pipeline, iters, breakdown);

    std::cout << "BENCH_EVENTS " << summary.n_events << "\n";
    std::cout << "BENCH_EPS_MIN " << summary.eps_min << "\n";
    std::cout << "BENCH_EPS_MEDIAN " << summary.eps_median << "\n";
    std::cout << "BENCH_EPS_MAX " << summary.eps_max << "\n";

    std::cout << "BENCH_NS_MIN " << summary.ns_min << "\n";
    std::cout << "BENCH_NS_MEDIAN " << summary.ns_median << "\n";
    std::cout << "BENCH_NS_MAX " << summary.ns_max << "\n";

    return 0;
  }

  hft::RunResult r1;
  hft::RunResult r2;

  if (pipeline) {
    r1 = hft::pipeline_run(loaded);
    r2 = hft::pipeline_run(loaded);
  } else {
    // Run #1 (from loaded tape)
    hft::Engine e1;
    r1 = e1.run(loaded);

    // Run #2 (replay same loaded tape into a fresh engine)
    hft::Engine e2;
    r2 = e2.run(loaded);
  }
  const auto sig1 = hft::fills_signature(r1.fills);
  const auto sig2 = hft::fills_signature(r2.fills);

  std::cout << "RUN1_SIG " << sig1 << "\n";
  std::cout << "RUN2_SIG " << sig2 << "\n";
  std::cout << "RUN1_TOP " << r1.final_top << "\n";
  std::cout << "RUN2_TOP " << r2.final_top << "\n";

  if (hft::fills_equal(r1.fills, r2.fills) && r1.final_top == r2.final_top) {
    std::cout << "REPLAY_OK\n";
    return 0;
  }

  std::cout << "REPLAY_MISMATCH\n";
  return 1;
}
