#include "hft/engine.hpp"
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

int main(int argc, char** argv) {
  const std::string tape_path = get_tape_path(argc, argv);

  std::cout << "TAPE_PATH " << tape_path << "\n";

  const bool exists = std::filesystem::exists(tape_path);
  if (!exists) {
    const auto events = default_scenario();
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

