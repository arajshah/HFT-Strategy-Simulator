#pragma once

#include "hft/order.hpp"

#include <cstdint>
#include <optional>
#include <string>
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

 private:
  // Minimal L1/L2-ish structure for Phase 0:
  // We keep explicit orders in vectors and match by price-time priority within best price.
  // This is intentionally simple; we’ll replace with price-level queues later.

  std::vector<Order> bids_;  // buy orders
  std::vector<Order> asks_;  // sell orders

  static bool is_live(const Order& o) { return o.qty > 0; }

  // Finds best index on a side (max bid, min ask). Returns npos if none live.
  static std::size_t find_best_bid_idx(const std::vector<Order>& bids);
  static std::size_t find_best_ask_idx(const std::vector<Order>& asks);
};

}  // namespace hft

