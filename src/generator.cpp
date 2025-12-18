#include "hft/generator.hpp"

#include "hft/order.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace hft {
namespace {

// xorshift64* (deterministic, tiny, stable)
struct Rng {
  std::uint64_t s;

  explicit Rng(std::uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ULL) {}

  std::uint64_t next_u64() {
    std::uint64_t x = s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    s = x;
    return x * 0x2545F4914F6CDD1DULL;
  }

  std::uint64_t uniform_u64(std::uint64_t lo, std::uint64_t hi) {
    // inclusive range; deterministic modulo (bias acceptable for simulation v1)
    const std::uint64_t span = (hi - lo + 1);
    return lo + (next_u64() % span);
  }

  std::int64_t uniform_i64(std::int64_t lo, std::int64_t hi) {
    const std::uint64_t ulo = static_cast<std::uint64_t>(lo);
    const std::uint64_t uhi = static_cast<std::uint64_t>(hi);
    return static_cast<std::int64_t>(uniform_u64(ulo, uhi));
  }

  bool bernoulli(std::uint64_t num, std::uint64_t den) {
    return uniform_u64(1, den) <= num;
  }
};

static void live_add(std::vector<Order::OrderId>& ids,
                     std::unordered_map<Order::OrderId, std::size_t>& pos,
                     Order::OrderId id) {
  pos[id] = ids.size();
  ids.push_back(id);
}

static bool live_pop_random(Rng& rng,
                            std::vector<Order::OrderId>& ids,
                            std::unordered_map<Order::OrderId, std::size_t>& pos,
                            Order::OrderId& out) {
  if (ids.empty()) return false;
  const std::size_t idx = static_cast<std::size_t>(rng.uniform_u64(0, ids.size() - 1));
  out = ids[idx];

  const Order::OrderId last = ids.back();
  ids[idx] = last;
  ids.pop_back();

  pos[last] = idx;
  pos.erase(out);
  return true;
}

}  // namespace

std::vector<Event> generate_events_gen_v1(std::uint64_t seed,
                                         std::size_t n_events,
                                         std::int64_t mid_px,
                                         std::int64_t spread_ticks) {
  Rng rng(seed);

  std::vector<Event> events;
  events.reserve(n_events + 32);

  std::uint64_t ts = 1;
  std::uint64_t seq = 0;
  Order::OrderId next_id = 1;

  // Track "submitted live" IDs for generating cancels (may be stale at runtime due to fills; OK).
  std::vector<Order::OrderId> live_ids;
  std::unordered_map<Order::OrderId, std::size_t> live_pos;
  live_ids.reserve(n_events / 2 + 16);
  live_pos.reserve(n_events / 2 + 16);

  // --- Initial deterministic seeding: symmetric book around mid ---
  // Asks: mid..mid+2 ; Bids: mid-2..mid-1
  for (int i = 0; i < 3; ++i) {
    const std::int64_t px = mid_px + i;
    const std::int64_t qty = 50 + static_cast<std::int64_t>(i) * 10;
    Order o{.id = next_id++, .side = Side::Sell, .price_ticks = px, .qty = qty};
    events.push_back(Event::add_limit(ts, ++seq, o));
    live_add(live_ids, live_pos, o.id);
  }
  for (int i = 2; i >= 1; --i) {
    const std::int64_t px = mid_px - i;
    const std::int64_t qty = 50 + static_cast<std::int64_t>(i) * 10;
    Order o{.id = next_id++, .side = Side::Buy, .price_ticks = px, .qty = qty};
    events.push_back(Event::add_limit(ts, ++seq, o));
    live_add(live_ids, live_pos, o.id);
  }

  // --- Generated orderflow ---
  // For each step:
  //  - 25% cancel (if we have ids), else add
  //  - Adds are 10% aggressive (cross) to create fills; otherwise passive near mid.
  for (std::size_t i = 0; i < n_events; ++i) {
    ++ts;

    const bool do_cancel = (!live_ids.empty() && rng.bernoulli(1, 4));  // 25%
    if (do_cancel) {
      Order::OrderId cid = 0;
      (void)live_pop_random(rng, live_ids, live_pos, cid);
      events.push_back(Event::cancel(ts, ++seq, cid));
      continue;
    }

    // Add
    const Side side = rng.bernoulli(1, 2) ? Side::Buy : Side::Sell;
    const bool aggressive = rng.bernoulli(1, 10);  // 10%
    const std::int64_t dist = rng.uniform_i64(0, std::max<std::int64_t>(1, spread_ticks));

    std::int64_t px = mid_px;
    if (side == Side::Buy) {
      px = aggressive ? (mid_px + dist) : (mid_px - dist);
    } else {
      px = aggressive ? (mid_px - dist) : (mid_px + dist);
    }

    const std::int64_t qty = rng.uniform_i64(1, 50);

    Order o{.id = next_id++, .side = side, .price_ticks = px, .qty = qty};
    events.push_back(Event::add_limit(ts, ++seq, o));
    live_add(live_ids, live_pos, o.id);
  }

  return events;
}

}  // namespace hft

