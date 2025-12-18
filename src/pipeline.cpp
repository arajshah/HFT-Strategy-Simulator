#include "hft/pipeline.hpp"

#include "hft/order_book.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>

namespace hft {
namespace {

template <typename T>
class BlockingQueue {
 public:
  void push(T v) {
    {
      std::lock_guard<std::mutex> lk(mu_);
      q_.push_back(std::move(v));
    }
    cv_.notify_one();
  }

  T pop() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return !q_.empty(); });
    T v = std::move(q_.front());
    q_.pop_front();
    return v;
  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<T> q_;
};

}  // namespace

RunResult pipeline_run(std::vector<Event> events) {
  // Deterministic ordering identical to Engine::run
  std::sort(events.begin(), events.end(),
            [](const Event& a, const Event& b) {
              if (a.ts != b.ts) return a.ts < b.ts;
              return a.seq < b.seq;
            });

  BlockingQueue<std::optional<Event>> q;

  RunResult out;

  // Consumer: sole writer of the book
  std::thread consumer([&]() {
    OrderBook book;

    while (true) {
      std::optional<Event> item = q.pop();
      if (!item.has_value()) break;

      const Event& e = *item;
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
      q.push(std::optional<Event>(e));
    }
    q.push(std::nullopt);  // sentinel
  });

  producer.join();
  consumer.join();
  return out;
}

}  // namespace hft

