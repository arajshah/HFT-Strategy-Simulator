// Event: a single timestamped action for the matching engine.
//
// An event is either an add-limit order or a cancel, identified by a
// timestamp and a sequence number that breaks ties deterministically.

#pragma once

#include "hft/order.hpp"

#include <cstdint>

namespace hft {

enum class EventType : std::uint8_t { AddLimit = 0, Cancel = 1 };

struct Event {
  std::uint64_t ts{};   // timestamp in arbitrary units (deterministic)
  std::uint64_t seq{};  // strict tie-breaker for same ts
  EventType type{EventType::AddLimit};

  // Payload (simple + explicit for Phase 1)
  Order order{};                 // used when type == AddLimit
  Order::OrderId cancel_id{0};   // used when type == Cancel

  static Event add_limit(std::uint64_t ts, std::uint64_t seq, const Order& o) {
    Event e;
    e.ts = ts;
    e.seq = seq;
    e.type = EventType::AddLimit;
    e.order = o;
    e.cancel_id = 0;
    return e;
  }

  static Event cancel(std::uint64_t ts, std::uint64_t seq, Order::OrderId id) {
    Event e;
    e.ts = ts;
    e.seq = seq;
    e.type = EventType::Cancel;
    e.order = Order{};
    e.cancel_id = id;
    return e;
  }
};

}  // namespace hft
