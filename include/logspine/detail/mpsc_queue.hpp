#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <utility>

namespace logspine::detail {

inline std::size_t round_up_to_power_of_2(std::size_t n) {
  if (n <= 1) return 2;
  --n;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n |= n >> 32;
  return n + 1;
}

template <typename T>
class mpsc_queue {
 public:
  explicit mpsc_queue(std::size_t capacity)
      : buffer_mask_(capacity - 1) {
    if ((capacity == 0) || ((capacity & (capacity - 1)) != 0)) {
      throw std::invalid_argument("capacity must be a power of 2");
    }
    buffer_ = new slot[capacity];
    for (std::size_t i = 0; i != capacity; ++i) {
      buffer_[i].sequence.store(i, std::memory_order_relaxed);
    }
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
  }

  ~mpsc_queue() {
    delete[] buffer_;
  }

  mpsc_queue(const mpsc_queue&) = delete;
  mpsc_queue& operator=(const mpsc_queue&) = delete;

  bool enqueue(T data) {
    slot* cell;
    std::size_t pos = head_.load(std::memory_order_relaxed);
    for (;;) {
      cell = &buffer_[pos & buffer_mask_];
      std::size_t seq = cell->sequence.load(std::memory_order_acquire);
      std::intptr_t dif = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);
      if (dif == 0) {
        if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          break;
        }
      } else if (dif < 0) {
        return false; // Full
      } else {
        pos = head_.load(std::memory_order_relaxed);
      }
    }
    cell->data = std::move(data);
    cell->sequence.store(pos + 1, std::memory_order_release);
    return true;
  }

  bool dequeue(T& data) {
    slot* cell;
    std::size_t pos = tail_.load(std::memory_order_relaxed);
    for (;;) {
      cell = &buffer_[pos & buffer_mask_];
      std::size_t seq = cell->sequence.load(std::memory_order_acquire);
      std::intptr_t dif = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos + 1);
      if (dif == 0) {
        if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          break;
        }
      } else if (dif < 0) {
        return false; // Empty
      } else {
        pos = tail_.load(std::memory_order_relaxed);
      }
    }
    data = std::move(cell->data);
    cell->sequence.store(pos + buffer_mask_ + 1, std::memory_order_release);
    return true;
  }

 private:
  struct slot {
    std::atomic<std::size_t> sequence;
    T data;
  };

  alignas(64) std::size_t buffer_mask_;
  slot* buffer_;
  alignas(64) std::atomic<std::size_t> head_;
  alignas(64) std::atomic<std::size_t> tail_;
};

} // namespace logspine::detail
