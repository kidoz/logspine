#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <variant>
#include <vector>

#include <logspine/dispatcher.hpp>
#include <logspine/sink.hpp>
#include <logspine/detail/mpsc_queue.hpp>

namespace logspine {

enum class overflow_policy {
  block,
  drop_newest,
  drop_oldest,
};

struct async_options {
  std::size_t queue_capacity = 8192;
  overflow_policy overflow = overflow_policy::drop_newest;
  std::size_t batch_size = 64;
  std::chrono::milliseconds block_retry_timeout = std::chrono::milliseconds(5000);
};

class async_dispatcher final : public dispatcher {
 public:
  async_dispatcher(std::vector<std::shared_ptr<sink>> sinks, async_options options = {});
  ~async_dispatcher() override;

  async_dispatcher(const async_dispatcher&) = delete;
  async_dispatcher& operator=(const async_dispatcher&) = delete;

  void dispatch(log_event event) override;
  void flush() override;
  [[nodiscard]] std::uint64_t dropped_events() const noexcept override;
  [[nodiscard]] std::uint64_t sink_failures() const noexcept override;

 private:
  struct flush_request {
    std::uint64_t id = 0;
  };

  using queue_item = std::variant<log_event, flush_request>;

  void worker_loop();
  bool enqueue_event(log_event event);
  void enqueue_flush_request(std::uint64_t flush_id);
  void safe_write(const log_event& event) noexcept;
  void safe_flush_sinks() noexcept;
  void shutdown() noexcept;

  std::vector<std::shared_ptr<sink>> sinks_;
  async_options options_;
  std::unique_ptr<detail::mpsc_queue<queue_item>> queue_;

  mutable std::mutex mutex_;
  std::condition_variable queue_not_empty_;
  std::condition_variable queue_not_full_;
  std::thread worker_;

  std::atomic<bool> stopping_{false};
  std::atomic<bool> sleeping_{false};

  std::atomic<std::uint64_t> completed_flush_id_{0};
  std::atomic<std::uint64_t> next_flush_id_{0};
  std::atomic<std::uint64_t> dropped_events_{0};
  std::atomic<std::uint64_t> sink_failures_{0};
};

}  // namespace logspine
