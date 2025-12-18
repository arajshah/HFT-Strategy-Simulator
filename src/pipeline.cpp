#include "hft/pipeline.hpp"

#include "hft/order_book.hpp"
#include "hft/spsc_ring.hpp"

#include <algorithm>
#include <thread>

namespace hft {
namespace {

struct PipeMsg {
  bool is_end{false};
  Event e{};
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
  // Power-of-two capacity; small enough to avoid huge memory, large enough for throughput.
  SpscRing<PipeMsg> q(1u << 16);

  RunResult out;

  // Consumer: sole writer of the book
  std::thread consumer([&]() {
    OrderBook book;

    PipeMsg msg{};
    while (true) {
      while (!q.try_pop(msg)) {
        std::this_thread::yield();
      }

      if (msg.is_end) break;

      const Event& e = msg.e;
      switch (e.type) {
        case EventType::AddLimit: {
          auto fills = book.add_limit(e.order);
          out.fills.insert(out.fills.end(), fills.begin(), fills.end());
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

    out.final_top = book.to_string_top();
  });

  // Producer: feeds already-sorted events
  std::thread producer([&]() {
    for (const auto& e : events) {
      PipeMsg m;
      m.is_end = false;
      m.e = e;
      while (!q.try_push(m)) {
        std::this_thread::yield();
      }
    }
    PipeMsg end;
    end.is_end = true;
    while (!q.try_push(end)) {
      std::this_thread::yield();
    }
  });

  producer.join();
  consumer.join();
  return out;
}

}  // namespace hft

