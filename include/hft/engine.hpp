#pragma once

#include "hft/event.hpp"
#include "hft/order_book.hpp"

#include <string>
#include <vector>

namespace hft {

struct RunResult {
  std::vector<Fill> fills;
  std::string final_top;
};

class Engine {
 public:
  // Takes events by value so we can sort deterministically inside.
  RunResult run(std::vector<Event> events);

  const OrderBook& book() const { return book_; }

 private:
  OrderBook book_;
};

bool fills_equal(const std::vector<Fill>& a, const std::vector<Fill>& b);
std::string fills_signature(const std::vector<Fill>& fills);

}  // namespace hft

