#include "hft/order_book.hpp"

#include <iostream>

int main() {
  hft::OrderBook ob;

  // Seed scenario (deterministic, hard-coded for Phase 0).
  // Prices are "ticks" (e.g. cents). Higher = more expensive.
  // 1) Add some resting liquidity
  ob.add_limit(hft::Order{.id = 1, .side = hft::Side::Sell, .price_ticks = 101, .qty = 50});
  ob.add_limit(hft::Order{.id = 2, .side = hft::Side::Sell, .price_ticks = 102, .qty = 50});
  ob.add_limit(hft::Order{.id = 3, .side = hft::Side::Buy,  .price_ticks =  99, .qty = 40});
  ob.add_limit(hft::Order{.id = 4, .side = hft::Side::Buy,  .price_ticks = 100, .qty = 40});

  std::cout << ob.to_string_top() << "\n";

  // 2) Submit an aggressive buy that crosses the ask at 101
  auto fills = ob.add_limit(hft::Order{.id = 5, .side = hft::Side::Buy, .price_ticks = 101, .qty = 30});
  for (const auto& f : fills) {
    std::cout << "FILL taker=" << f.taker_id
              << " maker=" << f.maker_id
              << " px=" << f.price_ticks
              << " qty=" << f.qty << "\n";
  }

  std::cout << ob.to_string_top() << "\n";

  // 3) Cancel a resting order
  const bool cancelled = ob.cancel(2);
  std::cout << "CANCEL id=2 -> " << (cancelled ? "true" : "false") << "\n";
  std::cout << ob.to_string_top() << "\n";

  return 0;
}

