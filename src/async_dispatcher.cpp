#include <logspine/async_dispatcher.hpp>

#include <stdexcept>
#include <utility>

namespace logspine {

async_dispatcher::async_dispatcher(std::vector<std::shared_ptr<sink>> sinks, async_options options)
    : sinks_(std::move(sinks)), options_(options) {
  if (options_.queue_capacity == 0U) {
    throw std::invalid_argument("async_dispatcher queue_capacity must be greater than zero");
  }
  if (options_.batch_size == 0U) {
    throw std::invalid_argument("async_dispatcher batch_size must be greater than zero");
  }

  worker_ = std::thread(&async_dispatcher::worker_loop, this);
}

async_dispatcher::~async_dispatcher() { shutdown(); }

void async_dispatcher::dispatch(log_event event) { static_cast<void>(enqueue_event(std::move(event))); }

void async_dispatcher::flush() {
  const auto flush_id = next_flush_id_.fetch_add(1, std::memory_order_relaxed) + 1;
  enqueue_flush_request(flush_id);

  std::unique_lock lock(mutex_);
  flush_completed_.wait(lock, [this, flush_id] { return completed_flush_id_ >= flush_id; });
}

std::uint64_t async_dispatcher::dropped_events() const noexcept { return dropped_events_.load(std::memory_order_relaxed); }

std::uint64_t async_dispatcher::sink_failures() const noexcept { return sink_failures_.load(std::memory_order_relaxed); }

void async_dispatcher::worker_loop() {
  for (;;) {
    std::deque<queue_item> batch;

    {
      std::unique_lock lock(mutex_);
      queue_not_empty_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
      if (queue_.empty() && stopping_) {
        break;
      }

      const auto batch_limit = options_.batch_size;
      while (!queue_.empty() && batch.size() < batch_limit) {
        batch.push_back(std::move(queue_.front()));
        queue_.pop_front();
      }
      queue_not_full_.notify_all();
    }

    for (auto& item : batch) {
      if (std::holds_alternative<log_event>(item)) {
        safe_write(std::get<log_event>(item));
      } else {
        safe_flush_sinks();

        {
          std::scoped_lock lock(mutex_);
          completed_flush_id_ = std::max(completed_flush_id_, std::get<flush_request>(item).id);
        }
        flush_completed_.notify_all();
      }
    }
  }

  safe_flush_sinks();
}

bool async_dispatcher::enqueue_event(log_event event) {
  std::unique_lock lock(mutex_);

  while (!stopping_ && queue_.size() >= options_.queue_capacity) {
    switch (options_.overflow) {
      case overflow_policy::block:
        queue_not_full_.wait(lock, [this] { return stopping_ || queue_.size() < options_.queue_capacity; });
        break;
      case overflow_policy::drop_newest:
        dropped_events_.fetch_add(1, std::memory_order_relaxed);
        return false;
      case overflow_policy::drop_oldest:
        if (!try_drop_oldest_event_locked()) {
          dropped_events_.fetch_add(1, std::memory_order_relaxed);
          return false;
        }
        break;
    }
  }

  if (stopping_) {
    dropped_events_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  queue_.push_back(std::move(event));
  queue_not_empty_.notify_one();
  return true;
}

void async_dispatcher::enqueue_flush_request(std::uint64_t flush_id) {
  std::unique_lock lock(mutex_);
  queue_not_full_.wait(lock, [this] { return stopping_ || queue_.size() < options_.queue_capacity; });
  if (stopping_) {
    completed_flush_id_ = std::max(completed_flush_id_, flush_id);
    flush_completed_.notify_all();
    return;
  }

  queue_.push_back(flush_request{flush_id});
  queue_not_empty_.notify_one();
}

bool async_dispatcher::try_drop_oldest_event_locked() {
  for (auto it = queue_.begin(); it != queue_.end(); ++it) {
    if (std::holds_alternative<log_event>(*it)) {
      queue_.erase(it);
      dropped_events_.fetch_add(1, std::memory_order_relaxed);
      return true;
    }
  }
  return false;
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
  {
    std::scoped_lock lock(mutex_);
    if (stopping_) {
      if (worker_.joinable()) {
        worker_.join();
      }
      return;
    }
    stopping_ = true;
  }

  queue_not_empty_.notify_all();
  queue_not_full_.notify_all();
  flush_completed_.notify_all();

  if (worker_.joinable()) {
    worker_.join();
  }
}

}  // namespace logspine
