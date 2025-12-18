#include "hft/order_book.hpp"

#include <algorithm>
#include <sstream>

namespace hft {

std::int64_t OrderBook::level_total_qty(const Queue& q) {
  std::int64_t total = 0;
  for (const auto& o : q) total += o.qty;
  return total;
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

  return fills;
}

bool OrderBook::cancel(Order::OrderId id) {
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
        if (q.empty()) bids_.erase(lvl_it);
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
        if (q.empty()) asks_.erase(lvl_it);
        return true;
      }
    }
  }

  // If we got here, index was stale.
  live_.erase(it);
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

