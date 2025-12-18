#pragma once

#include "hft/event.hpp"
#include "hft/order_book.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hft {

struct MarketView {
  std::size_t event_index{0};
  std::uint64_t ts{0};

  std::optional<std::int64_t> best_bid_px;
  std::optional<std::int64_t> best_ask_px;
};

enum class ActionType : std::uint8_t { AggressiveTrade = 0 };

struct Action {
  ActionType type{ActionType::AggressiveTrade};
  Side side{Side::Buy};
  std::int64_t qty{1};
};

class Strategy {
 public:
  virtual ~Strategy() = default;

  virtual std::string name() const = 0;

  // Called once before processing events.
  virtual void on_start(const MarketView&) {}

  // Called after each market event is applied.
  virtual void on_market_event(const Event&, const MarketView&) {}

  // Called after market-event callbacks, for the strategy to emit actions.
  virtual std::vector<Action> decide(const MarketView&) = 0;
};

// Baseline strategy: every N events, send one aggressive order (crosses spread).
class PeriodicTakerStrategy final : public Strategy {
 public:
  // side_mode: "buy" | "sell" | "alternate"
  PeriodicTakerStrategy(std::size_t every, std::int64_t qty, std::string side_mode)
      : every_(every ? every : 1), qty_(qty > 0 ? qty : 1), side_mode_(std::move(side_mode)) {}

  std::string name() const override { return "periodic_taker"; }

  std::vector<Action> decide(const MarketView& v) override {
    std::vector<Action> out;
    if (v.event_index == 0) return out;
    if ((v.event_index % every_) != 0) return out;

    Side s = Side::Buy;
    if (side_mode_ == "buy") s = Side::Buy;
    else if (side_mode_ == "sell") s = Side::Sell;
    else {  // alternate (default)
      s = flip_ ? Side::Sell : Side::Buy;
      flip_ = !flip_;
    }

    out.push_back(Action{.type = ActionType::AggressiveTrade, .side = s, .qty = qty_});
    return out;
  }

 private:
  std::size_t every_{1000};
  std::int64_t qty_{1};
  std::string side_mode_{"alternate"};
  bool flip_{false};
};

}  // namespace hft

