#include <logspine/async_dispatcher.hpp>

#include <stdexcept>
#include <utility>
#include <chrono>

namespace logspine {

async_dispatcher::async_dispatcher(std::vector<std::shared_ptr<sink>> sinks, async_options options)
    : sinks_(std::move(sinks)), options_(options) {
  if (options_.queue_capacity == 0U) {
    throw std::invalid_argument("async_dispatcher queue_capacity must be greater than zero");
  }
  if (options_.batch_size == 0U) {
    throw std::invalid_argument("async_dispatcher batch_size must be greater than zero");
  }

  const auto capacity = detail::round_up_to_power_of_2(options_.queue_capacity);
  queue_ = std::make_unique<detail::mpsc_queue<queue_item>>(capacity);

  worker_ = std::thread(&async_dispatcher::worker_loop, this);
}

async_dispatcher::~async_dispatcher() { shutdown(); }

void async_dispatcher::dispatch(log_event event) { static_cast<void>(enqueue_event(std::move(event))); }

void async_dispatcher::flush() {
  const auto flush_id = next_flush_id_.fetch_add(1, std::memory_order_relaxed) + 1;
  enqueue_flush_request(flush_id);

  while (completed_flush_id_.load(std::memory_order_acquire) < flush_id) {
    completed_flush_id_.wait(completed_flush_id_.load(std::memory_order_relaxed), std::memory_order_acquire);
  }
}

std::uint64_t async_dispatcher::dropped_events() const noexcept { return dropped_events_.load(std::memory_order_relaxed); }

std::uint64_t async_dispatcher::sink_failures() const noexcept { return sink_failures_.load(std::memory_order_relaxed); }

void async_dispatcher::worker_loop() {
  std::vector<queue_item> batch;
  batch.reserve(options_.batch_size);

  for (;;) {
    batch.clear();
    queue_item item;

    while (!stopping_.load(std::memory_order_acquire) && !queue_->dequeue(item)) {
      sleeping_.store(true, std::memory_order_release);
      if (queue_->dequeue(item)) {
        sleeping_.store(false, std::memory_order_release);
        break;
      }
      if (stopping_.load(std::memory_order_acquire)) break;
      std::unique_lock lock(mutex_);
      queue_not_empty_.wait_for(lock, std::chrono::milliseconds(50), [this] {
        return stopping_.load(std::memory_order_acquire) || !sleeping_.load(std::memory_order_acquire);
      });
    }

    if (stopping_.load(std::memory_order_acquire)) {
      while (queue_->dequeue(item)) {
        batch.push_back(std::move(item));
        if (batch.size() >= options_.batch_size) break;
      }
      if (batch.empty()) break;
    } else {
      batch.push_back(std::move(item));
      while (batch.size() < options_.batch_size && queue_->dequeue(item)) {
        batch.push_back(std::move(item));
      }
    }

    {
      std::scoped_lock lock(mutex_);
      queue_not_full_.notify_all();
    }

    for (auto& batch_item : batch) {
      if (std::holds_alternative<log_event>(batch_item)) {
        safe_write(std::get<log_event>(batch_item));
      } else {
        safe_flush_sinks();
        auto fid = std::get<flush_request>(batch_item).id;
        auto current = completed_flush_id_.load(std::memory_order_relaxed);
        while (current < fid && !completed_flush_id_.compare_exchange_weak(current, fid, std::memory_order_release, std::memory_order_relaxed)) {}
        completed_flush_id_.notify_all();
      }
    }
  }

  safe_flush_sinks();
}

bool async_dispatcher::enqueue_event(log_event event) {
  if (stopping_.load(std::memory_order_acquire)) {
    dropped_events_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  if (queue_->enqueue(event)) {
    if (sleeping_.load(std::memory_order_acquire)) {
      std::scoped_lock lock(mutex_);
      sleeping_.store(false, std::memory_order_release);
      queue_not_empty_.notify_one();
    }
    return true;
  }

  auto start = std::chrono::steady_clock::now();
  std::unique_lock lock(mutex_, std::defer_lock);

  for (;;) {
    if (queue_->enqueue(event)) {
      if (sleeping_.load(std::memory_order_acquire)) {
        if (!lock.owns_lock()) lock.lock();
        sleeping_.store(false, std::memory_order_release);
        queue_not_empty_.notify_one();
      }
      return true;
    }

    if (stopping_.load(std::memory_order_acquire)) {
      dropped_events_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    switch (options_.overflow) {
      case overflow_policy::drop_newest:
        dropped_events_.fetch_add(1, std::memory_order_relaxed);
        return false;
      case overflow_policy::drop_oldest: {
        queue_item dummy;
        if (queue_->dequeue(dummy)) {
          dropped_events_.fetch_add(1, std::memory_order_relaxed);
          if (std::holds_alternative<flush_request>(dummy)) {
            enqueue_flush_request(std::get<flush_request>(dummy).id);
          }
        }
        break; // retry immediately
      }
      case overflow_policy::block: {
        if (!lock.owns_lock()) lock.lock();
        if (options_.block_retry_timeout.count() > 0) {
          if (queue_not_full_.wait_for(lock, std::chrono::milliseconds(10)) == std::cv_status::timeout) {
            if (std::chrono::steady_clock::now() - start >= options_.block_retry_timeout) {
              dropped_events_.fetch_add(1, std::memory_order_relaxed);
              return false;
            }
          }
        } else {
          queue_not_full_.wait_for(lock, std::chrono::milliseconds(10));
        }
        break;
      }
    }
  }
}

void async_dispatcher::enqueue_flush_request(std::uint64_t flush_id) {
  if (stopping_.load(std::memory_order_acquire)) {
    auto current = completed_flush_id_.load(std::memory_order_relaxed);
    while (current < flush_id && !completed_flush_id_.compare_exchange_weak(current, flush_id, std::memory_order_release, std::memory_order_relaxed)) {}
    completed_flush_id_.notify_all();
    return;
  }

  queue_item item = flush_request{flush_id};
  std::unique_lock lock(mutex_, std::defer_lock);

  for (;;) {
    if (queue_->enqueue(item)) {
      if (sleeping_.load(std::memory_order_acquire)) {
        if (!lock.owns_lock()) lock.lock();
        sleeping_.store(false, std::memory_order_release);
        queue_not_empty_.notify_one();
      }
      return;
    }

    if (stopping_.load(std::memory_order_acquire)) {
      auto current = completed_flush_id_.load(std::memory_order_relaxed);
      while (current < flush_id && !completed_flush_id_.compare_exchange_weak(current, flush_id, std::memory_order_release, std::memory_order_relaxed)) {}
      completed_flush_id_.notify_all();
      return;
    }

    if (!lock.owns_lock()) lock.lock();
    queue_not_full_.wait_for(lock, std::chrono::milliseconds(10));
  }
}

void async_dispatcher::safe_write(const log_event& event) noexcept {
  for (const auto& current_sink : sinks_) {
    try {
      current_sink->write(event);
    } catch (...) {
      sink_failures_.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

void async_dispatcher::safe_flush_sinks() noexcept {
  for (const auto& current_sink : sinks_) {
    try {
      current_sink->flush();
    } catch (...) {
      sink_failures_.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

void async_dispatcher::shutdown() noexcept {
  bool should_join = false;
  if (!stopping_.exchange(true, std::memory_order_acq_rel)) {
    should_join = true;
  }

  if (should_join) {
    {
      std::scoped_lock lock(mutex_);
      sleeping_.store(false, std::memory_order_release);
      queue_not_empty_.notify_all();
      queue_not_full_.notify_all();
    }
    
    auto current = completed_flush_id_.load(std::memory_order_relaxed);
    while (!completed_flush_id_.compare_exchange_weak(current, std::numeric_limits<std::uint64_t>::max(), std::memory_order_release, std::memory_order_relaxed)) {}
    completed_flush_id_.notify_all();

    if (worker_.joinable()) {
      worker_.join();
    }
  }
}

}  // namespace logspine