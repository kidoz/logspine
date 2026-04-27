#pragma once

#include <cstdint>

#include <logspine/log_event.hpp>

namespace logspine {

class dispatcher {
 public:
  virtual ~dispatcher() = default;

  virtual void dispatch(log_event event) = 0;
  virtual void flush() = 0;
  [[nodiscard]] virtual std::uint64_t dropped_events() const noexcept = 0;
  [[nodiscard]] virtual std::uint64_t sink_failures() const noexcept = 0;
};

}  // namespace logspine
