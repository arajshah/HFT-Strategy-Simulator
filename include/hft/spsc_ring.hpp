#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace hft {

// Lock-free SPSC ring buffer (single producer, single consumer).
// - Capacity must be a power of two.
// - T must be trivially copyable (we store by value).
template <typename T>
class SpscRing {
 public:
  explicit SpscRing(std::size_t capacity_pow2)
      : cap_(capacity_pow2),
        mask_(capacity_pow2 - 1),
        buf_(capacity_pow2) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "SpscRing requires trivially copyable T");
  }

  bool try_push(const T& v) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t next = (tail + 1) & mask_;
    const std::size_t head = head_.load(std::memory_order_acquire);
    if (next == head) return false;  // full

    buf_[tail] = v;
    tail_.store(next, std::memory_order_release);
    return true;
  }

  bool try_pop(T& out) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    if (head == tail) return false;  // empty

    out = buf_[head];
    head_.store((head + 1) & mask_, std::memory_order_release);
    return true;
  }

 private:
  const std::size_t cap_;
  const std::size_t mask_;
  std::vector<T> buf_;

  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

}  // namespace hft

