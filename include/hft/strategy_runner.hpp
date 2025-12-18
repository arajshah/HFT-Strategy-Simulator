#pragma once

#include "hft/event.hpp"
#include "hft/engine.hpp"     // RunResult
#include "hft/strategy.hpp"   // Strategy

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hft {

struct StrategyRunResult {
  std::string strategy_name;

  std::size_t n_actions{0};
  std::size_t n_fills{0};
  std::int64_t traded_qty{0};

  std::int64_t final_position{0};  // +buy, -sell (in qty units)
  double final_mid{0.0};
  double cash{0.0};                // in ticks*qty units
  double pnl_mtm{0.0};             // cash + position*mid

  std::string final_top;
};

// Runs the tape through a single OrderBook.
// Market events are applied, then strategy may emit actions which are applied immediately.
// Strategy orders use IDs strictly greater than any order_id seen in the tape.
StrategyRunResult run_strategy(const std::vector<Event>& tape_events, Strategy& strat);

}  // namespace hft

