#pragma once

#include <cstdint>

namespace logspine::sinks {

struct network_sink_statistics {
  std::uint64_t write_attempts = 0;
  std::uint64_t write_failures = 0;
  std::uint64_t reconnect_attempts = 0;
};

} // namespace logspine::sinks
