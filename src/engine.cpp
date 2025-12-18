#include "hft/engine.hpp"

#include <algorithm>
#include <sstream>

namespace hft {

RunResult Engine::run(std::vector<Event> events) {
  // Deterministic ordering: (ts, seq)
  std::sort(events.begin(), events.end(),
            [](const Event& a, const Event& b) {
              if (a.ts != b.ts) return a.ts < b.ts;
              return a.seq < b.seq;
            });

  RunResult out;

  for (const auto& e : events) {
    switch (e.type) {
      case EventType::AddLimit: {
        auto fills = book_.add_limit(e.order);
        out.fills.insert(out.fills.end(), fills.begin(), fills.end());
        break;
      }
      case EventType::Cancel: {
        (void)book_.cancel(e.cancel_id);
        break;
      }
      default:
        break;
    }
  }

  out.final_top = book_.to_string_top();
  return out;
}

bool fills_equal(const std::vector<Fill>& a, const std::vector<Fill>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].taker_id != b[i].taker_id) return false;
    if (a[i].maker_id != b[i].maker_id) return false;
    if (a[i].price_ticks != b[i].price_ticks) return false;
    if (a[i].qty != b[i].qty) return false;
  }
  return true;
}

std::string fills_signature(const std::vector<Fill>& fills) {
  std::ostringstream oss;
  // Compact deterministic signature
  for (const auto& f : fills) {
    oss << f.taker_id << ":" << f.maker_id << "@"
        << f.price_ticks << "x" << f.qty << ";";
  }
  return oss.str();
}

}  // namespace hft

