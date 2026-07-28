#include <logspine/sync_dispatcher.hpp>

#include <utility>

namespace logspine {

sync_dispatcher::sync_dispatcher(std::vector<std::shared_ptr<sink>> sinks) : sinks_(std::move(sinks)) {}

void sync_dispatcher::dispatch(log_event event) {
  std::scoped_lock lock(mutex_);
  for (const auto& current_sink : sinks_) {
    try {
      current_sink->write(event);
    } catch (...) {
      ++sink_failures_;
    }
  }
}

void sync_dispatcher::flush() {
  std::scoped_lock lock(mutex_);
  for (const auto& current_sink : sinks_) {
    try {
      current_sink->flush();
    } catch (...) {
      ++sink_failures_;
    }
  }
}

std::uint64_t sync_dispatcher::dropped_events() const noexcept {
  return 0;
}

std::uint64_t sync_dispatcher::sink_failures() const noexcept {
  std::scoped_lock lock(mutex_);
  return sink_failures_;
}

} // namespace logspine
