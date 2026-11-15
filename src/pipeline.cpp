// Pipeline: replays the event stream through the staged pipeline version
// of the engine instead of the single-threaded run.

#include "hft/pipeline.hpp"

#include "hft/order_book.hpp"
#include "hft/spsc_ring.hpp"

#include <algorithm>
#include <cstdint>
#include <thread>
#include <vector>

namespace hft {
namespace {

struct PipeMsg {
  bool is_end{false};
  std::uint32_t idx{0};  // index into the sorted events vector
};

}  // namespace

RunResult pipeline_run(std::vector<Event> events) {
  // Deterministic ordering identical to Engine::run
  std::sort(events.begin(), events.end(),
            [](const Event& a, const Event& b) {
              if (a.ts != b.ts) return a.ts < b.ts;
              return a.seq < b.seq;
            });

  // Lock-free SPSC queue between producer and consumer.
  SpscRing<PipeMsg> q(1u << 16);

  RunResult out;

  // Consumer: sole writer of the book and producer of fills
  std::thread consumer([&]() {
    OrderBook book;
    std::vector<Fill> fills;
    fills.reserve(1024);  // small seed reserve to reduce early reallocs

    PipeMsg msg{};
    while (true) {
      while (!q.try_pop(msg)) {
        std::this_thread::yield();
      }

      if (msg.is_end) break;

      const Event& e = events[msg.idx];
      switch (e.type) {
        case EventType::AddLimit: {
          auto new_fills = book.add_limit(e.order);
          if (!new_fills.empty()) {
            fills.insert(fills.end(), new_fills.begin(), new_fills.end());
          }
          break;
        }
        case EventType::Cancel: {
          (void)book.cancel(e.cancel_id);
          break;
        }
        default:
          break;
      }
    }

    out.fills = std::move(fills);
    out.final_top = book.to_string_top();
  });

  // Producer: feeds already-sorted events by index (avoids copying Event into the ring)
  std::thread producer([&]() {
    const std::size_t n = events.size();
    for (std::size_t i = 0; i < n; ++i) {
      PipeMsg m;
      m.is_end = false;
      m.idx = static_cast<std::uint32_t>(i);
      while (!q.try_push(m)) {
        std::this_thread::yield();
      }
    }

    PipeMsg end;
    end.is_end = true;
    end.idx = 0;
    while (!q.try_push(end)) {
      std::this_thread::yield();
    }
  });

  producer.join();
  consumer.join();
  return out;
}

}  // namespace hft
