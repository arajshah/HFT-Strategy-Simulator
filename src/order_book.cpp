#include "hft/order_book.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>
#include <unordered_set>

namespace hft {

std::int64_t OrderBook::level_total_qty(const Queue& q) {
  std::int64_t total = 0;
  for (const auto& o : q) total += o.qty;
  return total;
}

bool OrderBook::has_live_id(Order::OrderId id) const {
  return live_.find(id) != live_.end();
}

void OrderBook::erase_level_if_empty(Side side, std::int64_t price_ticks) {
  if (side == Side::Buy) {
    auto it = bids_.find(price_ticks);
    if (it != bids_.end() && it->second.empty()) bids_.erase(it);
  } else {
    auto it = asks_.find(price_ticks);
    if (it != asks_.end() && it->second.empty()) asks_.erase(it);
  }
}

void OrderBook::debug_assert_invariants() const {
#ifndef NDEBUG
  // 1) No empty queues stored in the maps.
  for (const auto& [px, q] : bids_) {
    assert(!q.empty() && "bids_ contains an empty price level queue");
    (void)px;
  }
  for (const auto& [px, q] : asks_) {
    assert(!q.empty() && "asks_ contains an empty price level queue");
    (void)px;
  }

  // 2) All resting orders are live, have qty > 0, correct side, correct price level key,
  //    and have unique IDs. Also build the set of IDs observed in the book.
  std::unordered_set<Order::OrderId> seen_ids;
  seen_ids.reserve(live_.size() * 2 + 8);

  auto scan_side = [&](Side side) {
    if (side == Side::Buy) {
      for (const auto& [px, q] : bids_) {
        for (const auto& o : q) {
          assert(o.qty > 0 && "resting bid has non-positive qty");
          assert(o.side == Side::Buy && "resting bid has wrong side");
          assert(o.price_ticks == px && "resting bid price_ticks != level key");
          const bool inserted = seen_ids.insert(o.id).second;
          assert(inserted && "duplicate order id found in book");
          const auto it = live_.find(o.id);
          assert(it != live_.end() && "order in book missing from live_ index");
          assert(it->second.side == Side::Buy && "live_ side mismatch for resting bid");
          assert(it->second.price_ticks == px && "live_ price mismatch for resting bid");
        }
      }
    } else {
      for (const auto& [px, q] : asks_) {
        for (const auto& o : q) {
          assert(o.qty > 0 && "resting ask has non-positive qty");
          assert(o.side == Side::Sell && "resting ask has wrong side");
          assert(o.price_ticks == px && "resting ask price_ticks != level key");
          const bool inserted = seen_ids.insert(o.id).second;
          assert(inserted && "duplicate order id found in book");
          const auto it = live_.find(o.id);
          assert(it != live_.end() && "order in book missing from live_ index");
          assert(it->second.side == Side::Sell && "live_ side mismatch for resting ask");
          assert(it->second.price_ticks == px && "live_ price mismatch for resting ask");
        }
      }
    }
  };

  scan_side(Side::Buy);
  scan_side(Side::Sell);

  // 3) live_ should not contain ids not present in the book.
  for (const auto& [id, loc] : live_) {
    (void)loc;
    assert(seen_ids.find(id) != seen_ids.end() && "live_ contains id not present in book");
  }
#endif
}

std::optional<std::pair<std::int64_t, std::int64_t>> OrderBook::best_bid() const {
  if (bids_.empty()) return std::nullopt;
  const auto it = bids_.begin();
  if (it->second.empty()) return std::nullopt;
  return std::make_pair(it->first, level_total_qty(it->second));
}

std::optional<std::pair<std::int64_t, std::int64_t>> OrderBook::best_ask() const {
  if (asks_.empty()) return std::nullopt;
  const auto it = asks_.begin();
  if (it->second.empty()) return std::nullopt;
  return std::make_pair(it->first, level_total_qty(it->second));
}

std::vector<Fill> OrderBook::add_limit(const Order& incoming) {
  std::vector<Fill> fills;
  Order taker = incoming;

  debug_assert_invariants();

  // Reject nonsensical orders deterministically.
  if (incoming.qty <= 0) return {};

  // Disallow duplicate live order ids (deterministic no-op).
  if (has_live_id(incoming.id)) return {};

  auto crosses = [&](std::int64_t bid_px, std::int64_t ask_px) {
    return bid_px >= ask_px;
  };

  if (taker.side == Side::Buy) {
    // Match against asks (best ask = lowest price)
    while (taker.qty > 0 && !asks_.empty()) {
      auto best_it = asks_.begin();
      const std::int64_t best_ask_px = best_it->first;
      if (!crosses(taker.price_ticks, best_ask_px)) break;

      auto& q = best_it->second;
      while (taker.qty > 0 && !q.empty()) {
        Order& maker = q.front();
        const std::int64_t traded = std::min(taker.qty, maker.qty);

        fills.push_back(Fill{
            .taker_id = taker.id,
            .maker_id = maker.id,
            .price_ticks = best_ask_px,
            .qty = traded,
        });

        taker.qty -= traded;
        maker.qty -= traded;

        if (maker.qty == 0) {
          live_.erase(maker.id);
          q.pop_front();
        }
      }

      if (q.empty()) {
        asks_.erase(best_it);
      }
    }

    // Rest remaining as a bid at taker.price_ticks (FIFO at level)
    if (taker.qty > 0) {
      bids_[taker.price_ticks].push_back(taker);
      live_[taker.id] = Loc{.side = Side::Buy, .price_ticks = taker.price_ticks};
    }

  } else {  // Side::Sell
    // Match against bids (best bid = highest price)
    while (taker.qty > 0 && !bids_.empty()) {
      auto best_it = bids_.begin();
      const std::int64_t best_bid_px = best_it->first;
      if (!crosses(best_bid_px, taker.price_ticks)) break;

      auto& q = best_it->second;
      while (taker.qty > 0 && !q.empty()) {
        Order& maker = q.front();
        const std::int64_t traded = std::min(taker.qty, maker.qty);

        fills.push_back(Fill{
            .taker_id = taker.id,
            .maker_id = maker.id,
            .price_ticks = best_bid_px,
            .qty = traded,
        });

        taker.qty -= traded;
        maker.qty -= traded;

        if (maker.qty == 0) {
          live_.erase(maker.id);
          q.pop_front();
        }
      }

      if (q.empty()) {
        bids_.erase(best_it);
      }
    }

    // Rest remaining as an ask at taker.price_ticks (FIFO at level)
    if (taker.qty > 0) {
      asks_[taker.price_ticks].push_back(taker);
      live_[taker.id] = Loc{.side = Side::Sell, .price_ticks = taker.price_ticks};
    }
  }

  debug_assert_invariants();
  return fills;
}

bool OrderBook::cancel(Order::OrderId id) {
  debug_assert_invariants();

  const auto it = live_.find(id);
  if (it == live_.end()) return false;

  const Loc loc = it->second;

  if (loc.side == Side::Buy) {
    auto lvl_it = bids_.find(loc.price_ticks);
    if (lvl_it == bids_.end()) {
      live_.erase(it);
      return false;
    }

    auto& q = lvl_it->second;
    for (auto oq = q.begin(); oq != q.end(); ++oq) {
      if (oq->id == id) {
        q.erase(oq);
        live_.erase(it);
        erase_level_if_empty(Side::Buy, loc.price_ticks);
        debug_assert_invariants();
        return true;
      }
    }

  } else {  // Side::Sell
    auto lvl_it = asks_.find(loc.price_ticks);
    if (lvl_it == asks_.end()) {
      live_.erase(it);
      return false;
    }

    auto& q = lvl_it->second;
    for (auto oq = q.begin(); oq != q.end(); ++oq) {
      if (oq->id == id) {
        q.erase(oq);
        live_.erase(it);
        erase_level_if_empty(Side::Sell, loc.price_ticks);
        debug_assert_invariants();
        return true;
      }
    }
  }

#ifndef NDEBUG
  assert(false && "live_ index stale: id not found in indicated price level queue");
#endif
  // Self-heal in release builds.
  live_.erase(it);
  debug_assert_invariants();
  return false;
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

