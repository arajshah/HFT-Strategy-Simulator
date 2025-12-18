#pragma once

#include <cstdint>

namespace hft {

enum class Side : uint8_t { Buy = 0, Sell = 1 };

struct Order {
  using OrderId = std::uint64_t;

  OrderId id{};
  Side side{Side::Buy};
  std::int64_t price_ticks{};   // integer price (e.g., cents or ticks)
  std::int64_t qty{};           // remaining quantity
};

}  // namespace hft

