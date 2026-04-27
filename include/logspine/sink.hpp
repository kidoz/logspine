#pragma once

#include <cstdint>

#include <logspine/log_event.hpp>

namespace logspine {

enum class sink_format {
  human,
  json_lines,
};

class sink {
 public:
  virtual ~sink() = default;

  virtual void write(const log_event& event) = 0;
  virtual void flush() = 0;
};

}  // namespace logspine
