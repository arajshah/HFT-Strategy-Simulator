#include "hft/order_book.hpp"

#include <algorithm>
#include <limits>
#include <sstream>

namespace hft {

std::size_t OrderBook::find_best_bid_idx(const std::vector<Order>& bids) {
  std::size_t best = std::numeric_limits<std::size_t>::max();
  std::int64_t best_px = std::numeric_limits<std::int64_t>::min();

  for (std::size_t i = 0; i < bids.size(); ++i) {
    if (!is_live(bids[i])) continue;
    if (bids[i].price_ticks > best_px) {
      best_px = bids[i].price_ticks;
      best = i;
    }
  }
  return best;
}

std::size_t OrderBook::find_best_ask_idx(const std::vector<Order>& asks) {
  std::size_t best = std::numeric_limits<std::size_t>::max();
  std::int64_t best_px = std::numeric_limits<std::int64_t>::max();

  for (std::size_t i = 0; i < asks.size(); ++i) {
    if (!is_live(asks[i])) continue;
    if (asks[i].price_ticks < best_px) {
      best_px = asks[i].price_ticks;
      best = i;
    }
  }
  return best;
}

std::optional<std::pair<std::int64_t, std::int64_t>> OrderBook::best_bid() const {
  const auto idx = find_best_bid_idx(bids_);
  if (idx == std::numeric_limits<std::size_t>::max()) return std::nullopt;

  const auto px = bids_[idx].price_ticks;
  std::int64_t total = 0;
  for (const auto& o : bids_) if (is_live(o) && o.price_ticks == px) total += o.qty;
  return std::make_pair(px, total);
}

std::optional<std::pair<std::int64_t, std::int64_t>> OrderBook::best_ask() const {
  const auto idx = find_best_ask_idx(asks_);
  if (idx == std::numeric_limits<std::size_t>::max()) return std::nullopt;

  const auto px = asks_[idx].price_ticks;
  std::int64_t total = 0;
  for (const auto& o : asks_) if (is_live(o) && o.price_ticks == px) total += o.qty;
  return std::make_pair(px, total);
}

std::vector<Fill> OrderBook::add_limit(const Order& incoming) {
  std::vector<Fill> fills;

  // Copy so we can decrement remaining qty for this function.
  Order taker = incoming;

  auto& my_side = (taker.side == Side::Buy) ? bids_ : asks_;
  auto& opp_side = (taker.side == Side::Buy) ? asks_ : bids_;

  auto crosses = [&](std::int64_t bid_px, std::int64_t ask_px) {
    return bid_px >= ask_px;
  };

  // Match while we cross and have qty.
  while (taker.qty > 0) {
    const auto best_opp_idx =
        (taker.side == Side::Buy) ? find_best_ask_idx(opp_side) : find_best_bid_idx(opp_side);

    if (best_opp_idx == std::numeric_limits<std::size_t>::max()) break;

    Order& maker = opp_side[best_opp_idx];

    const std::int64_t best_bid_px = (taker.side == Side::Buy) ? taker.price_ticks : maker.price_ticks;
    const std::int64_t best_ask_px = (taker.side == Side::Buy) ? maker.price_ticks : taker.price_ticks;

    if (!crosses(best_bid_px, best_ask_px)) break;

    const std::int64_t traded = std::min(taker.qty, maker.qty);

    fills.push_back(Fill{
        .taker_id = taker.id,
        .maker_id = maker.id,
        .price_ticks = maker.price_ticks,  // pay the resting price (maker)
        .qty = traded,
    });

    taker.qty -= traded;
    maker.qty -= traded;
  }

  // If leftover, rest it as a live order.
  if (taker.qty > 0) {
    my_side.push_back(taker);
  }

  return fills;
}

bool OrderBook::cancel(Order::OrderId id) {
  auto kill = [&](std::vector<Order>& side) -> bool {
    for (auto& o : side) {
      if (o.id == id && is_live(o)) {
        o.qty = 0;
        return true;
      }
    }
    return false;
  };

  return kill(bids_) || kill(asks_);
}

std::string OrderBook::to_string_top() const {
  std::ostringstream oss;

  const auto bb = best_bid();
  const auto ba = best_ask();

  oss << "TOP ";
  if (bb) oss << "BID(" << bb->first << ", qty=" << bb->second << ") ";
  else oss << "BID(none) ";

  if (ba) oss << "ASK(" << ba->first << ", qty=" << ba->second << ")";
  else oss << "ASK(none)";

  return oss.str();
}

}  // namespace hft

