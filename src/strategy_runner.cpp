#include "hft/strategy_runner.hpp"

#include "hft/order.hpp"
#include "hft/order_book.hpp"

#include <algorithm>
#include <unordered_map>

namespace hft {
namespace {

static std::optional<std::int64_t> opt_px(const std::optional<std::pair<std::int64_t, std::int64_t>>& x) {
  if (!x) return std::nullopt;
  return x->first;
}

static double compute_mid(const OrderBook& book) {
  const auto bb = book.best_bid();
  const auto ba = book.best_ask();
  if (bb && ba) return 0.5 * (static_cast<double>(bb->first) + static_cast<double>(ba->first));
  if (bb) return static_cast<double>(bb->first);
  if (ba) return static_cast<double>(ba->first);
  return 0.0;
}

struct StratOrderInfo {
  Side side{Side::Buy};
};

}  // namespace

StrategyRunResult run_strategy(const std::vector<Event>& tape_events, Strategy& strat) {
  // Sort tape deterministically like Engine::run
  std::vector<Event> events = tape_events;
  std::sort(events.begin(), events.end(),
            [](const Event& a, const Event& b) {
              if (a.ts != b.ts) return a.ts < b.ts;
              return a.seq < b.seq;
            });

  // Determine starting strategy order id (avoid collision)
  Order::OrderId max_id = 0;
  for (const auto& e : events) {
    if (e.type == EventType::AddLimit) {
      if (e.order.id > max_id) max_id = e.order.id;
    } else if (e.type == EventType::Cancel) {
      if (e.cancel_id > max_id) max_id = e.cancel_id;
    }
  }
  Order::OrderId next_id = max_id + 1;

  OrderBook book;
  std::vector<Fill> all_fills;
  all_fills.reserve(1024);

  // Track strategy orders so we can compute PnL when they fill (as maker or taker)
  std::unordered_map<Order::OrderId, StratOrderInfo> strat_orders;
  strat_orders.reserve(1024);

  StrategyRunResult out;
  out.strategy_name = strat.name();

  MarketView view;
  view.event_index = 0;
  view.ts = 0;
  view.best_bid_px = opt_px(book.best_bid());
  view.best_ask_px = opt_px(book.best_ask());

  strat.on_start(view);

  for (std::size_t i = 0; i < events.size(); ++i) {
    const Event& e = events[i];

    // 1) Apply market event
    if (e.type == EventType::AddLimit) {
      auto fills = book.add_limit(e.order);
      if (!fills.empty()) {
        all_fills.insert(all_fills.end(), fills.begin(), fills.end());
      }
    } else if (e.type == EventType::Cancel) {
      (void)book.cancel(e.cancel_id);
    }

    // 2) Update view
    view.event_index = i + 1;
    view.ts = e.ts;
    view.best_bid_px = opt_px(book.best_bid());
    view.best_ask_px = opt_px(book.best_ask());

    // 3) Strategy callbacks + decisions
    strat.on_market_event(e, view);
    const auto actions = strat.decide(view);
    out.n_actions += actions.size();

    // 4) Apply actions immediately (deterministic)
    for (const auto& a : actions) {
      if (a.type != ActionType::AggressiveTrade) continue;
      if (a.qty <= 0) continue;

      Order o{};
      o.id = next_id++;
      o.side = a.side;
      o.qty = a.qty;

      // Set price to cross current top of book deterministically.
      // If no liquidity on the opposite side, place near mid so it rests.
      const auto bb = book.best_bid();
      const auto ba = book.best_ask();

      if (o.side == Side::Buy) {
        if (ba) o.price_ticks = ba->first;                  // cross at best ask
        else if (bb) o.price_ticks = bb->first + 1;         // step above bid
        else o.price_ticks = 0;
      } else { // Sell
        if (bb) o.price_ticks = bb->first;                  // cross at best bid
        else if (ba) o.price_ticks = ba->first - 1;         // step below ask
        else o.price_ticks = 0;
      }

      strat_orders[o.id] = StratOrderInfo{.side = o.side};

      auto fills = book.add_limit(o);
      if (!fills.empty()) {
        all_fills.insert(all_fills.end(), fills.begin(), fills.end());
      }
    }
  }

  // Compute PnL from fills involving strategy orders (maker or taker)
  double cash = 0.0;
  std::int64_t pos = 0;
  std::int64_t traded = 0;

  for (const auto& f : all_fills) {
    auto it_taker = strat_orders.find(f.taker_id);
    if (it_taker != strat_orders.end()) {
      const Side s = it_taker->second.side;
      const double px = static_cast<double>(f.price_ticks);
      const double q = static_cast<double>(f.qty);
      traded += f.qty;

      if (s == Side::Buy) { pos += f.qty; cash -= px * q; }
      else { pos -= f.qty; cash += px * q; }
      continue;
    }

    auto it_maker = strat_orders.find(f.maker_id);
    if (it_maker != strat_orders.end()) {
      const Side s = it_maker->second.side;
      const double px = static_cast<double>(f.price_ticks);
      const double q = static_cast<double>(f.qty);
      traded += f.qty;

      if (s == Side::Buy) { pos += f.qty; cash -= px * q; }
      else { pos -= f.qty; cash += px * q; }
      continue;
    }
  }

  out.n_fills = all_fills.size();
  out.traded_qty = traded;
  out.final_position = pos;
  out.cash = cash;

  out.final_mid = compute_mid(book);
  out.pnl_mtm = out.cash + (static_cast<double>(out.final_position) * out.final_mid);

  out.final_top = book.to_string_top();
  return out;
}

}  // namespace hft

