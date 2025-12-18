#include "hft/engine.hpp"

#include <iostream>
#include <vector>

int main() {
  // Deterministic event stream (no randomness in Phase 1)
  std::vector<hft::Event> events;
  std::uint64_t seq = 0;

  // Seed liquidity at ts=1
  events.push_back(hft::Event::add_limit(1, ++seq, hft::Order{.id = 1, .side = hft::Side::Sell, .price_ticks = 101, .qty = 50}));
  events.push_back(hft::Event::add_limit(1, ++seq, hft::Order{.id = 2, .side = hft::Side::Sell, .price_ticks = 102, .qty = 50}));
  events.push_back(hft::Event::add_limit(1, ++seq, hft::Order{.id = 3, .side = hft::Side::Buy,  .price_ticks =  99, .qty = 40}));
  events.push_back(hft::Event::add_limit(1, ++seq, hft::Order{.id = 4, .side = hft::Side::Buy,  .price_ticks = 100, .qty = 40}));

  // Aggressive buy at ts=2 (crosses best ask 101)
  events.push_back(hft::Event::add_limit(2, ++seq, hft::Order{.id = 5, .side = hft::Side::Buy, .price_ticks = 101, .qty = 30}));

  // Cancel a resting order at ts=3
  events.push_back(hft::Event::cancel(3, ++seq, 2));

  // Run #1
  hft::Engine e1;
  const auto r1 = e1.run(events);
  const auto sig1 = hft::fills_signature(r1.fills);

  // Run #2 (replay same events into a fresh engine)
  hft::Engine e2;
  const auto r2 = e2.run(events);
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

