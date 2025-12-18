#include "hft/bench.hpp"
#include "hft/engine.hpp"
#include "hft/generator.hpp"
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

  if (bench) {
    // Run once and print timing/throughput metrics.
    const auto br = hft::bench_run(loaded);

    std::cout << "BENCH_EVENTS " << br.n_events << "\n";
    std::cout << "BENCH_ADD " << br.n_add << "\n";
    std::cout << "BENCH_CANCEL " << br.n_cancel << "\n";
    std::cout << "BENCH_FILLS " << br.n_fills << "\n";
    std::cout << "BENCH_TRADED_QTY " << br.traded_qty << "\n";

    std::cout << "BENCH_SECONDS " << br.seconds << "\n";
    std::cout << "BENCH_EPS " << br.events_per_sec << "\n";
    std::cout << "BENCH_FPS " << br.fills_per_sec << "\n";
    std::cout << "BENCH_NS_PER_EVENT " << br.ns_per_event << "\n";

    return 0;
  }

  // Run #1 (from loaded tape)
  hft::Engine e1;
  const auto r1 = e1.run(loaded);
  const auto sig1 = hft::fills_signature(r1.fills);

  // Run #2 (replay same loaded tape into a fresh engine)
  hft::Engine e2;
  const auto r2 = e2.run(loaded);
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

