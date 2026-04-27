#pragma once

#include <iosfwd>
#include <mutex>

#include <logspine/sink.hpp>

namespace logspine::sinks {

struct console_sink_options {
  sink_format format = sink_format::human;
  bool use_stderr_for_warnings = true;
};

class console_sink final : public sink {
 public:
  explicit console_sink(console_sink_options options = {});

  void write(const log_event& event) override;
  void flush() override;

 private:
  [[nodiscard]] std::ostream& select_stream(level severity) const noexcept;

  console_sink_options options_;
  mutable std::mutex mutex_;
};

}  // namespace logspine::sinks
