#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include <logspine/dispatcher.hpp>
#include <logspine/sink.hpp>

namespace logspine {

class sync_dispatcher final : public dispatcher {
public:
  explicit sync_dispatcher(std::vector<std::shared_ptr<sink>> sinks);

  void dispatch(log_event event) override;
  void flush() override;
  [[nodiscard]] std::uint64_t dropped_events() const noexcept override;
  [[nodiscard]] std::uint64_t sink_failures() const noexcept override;

private:
  std::vector<std::shared_ptr<sink>> sinks_;
  mutable std::mutex mutex_;
  std::uint64_t sink_failures_ = 0;
};

} // namespace logspine
