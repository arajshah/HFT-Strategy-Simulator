// Price-time priority order book with best bid/ask lookup and cancel support.
#pragma once

#include "hft/order.hpp"

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hft {

struct Fill {
  Order::OrderId taker_id{};
  Order::OrderId maker_id{};
  std::int64_t price_ticks{};
  std::int64_t qty{};
};

class OrderBook {
 public:
  // Adds a limit order. Returns any fills generated immediately by crossing.
  std::vector<Fill> add_limit(const Order& o);

  // Cancels an existing live order by id. Returns true if found and cancelled.
  bool cancel(Order::OrderId id);

  // Best bid/ask as (price_ticks, total_qty_at_level). Empty if side has no liquidity.
  std::optional<std::pair<std::int64_t, std::int64_t>> best_bid() const;
  std::optional<std::pair<std::int64_t, std::int64_t>> best_ask() const;

  // Debug view (small string snapshot).
  std::string to_string_top() const;

  // Debug-only: asserts internal invariants when compiled without NDEBUG.
  void debug_assert_invariants() const;

 private:
  using Queue = std::list<Order>;

  // Price-level maps:
  // - bids_: highest price first
  // - asks_: lowest price first
  std::map<std::int64_t, Queue, std::greater<std::int64_t>> bids_;
  std::map<std::int64_t, Queue, std::less<std::int64_t>> asks_;

  struct Loc {
    Side side{};
    std::int64_t price_ticks{};
    Queue::iterator it{};  // iterator to the resting order in its price level
  };

  // Live order index: id -> (side, price_level)
  std::unordered_map<Order::OrderId, Loc> live_;

  static std::int64_t level_total_qty(const Queue& q);

  // Internal helpers
  bool has_live_id(Order::OrderId id) const;
  void erase_level_if_empty(Side side, std::int64_t price_ticks);
};

}  // namespace hft

