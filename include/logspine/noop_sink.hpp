#pragma once

#include <logspine/sink.hpp>

namespace logspine {

class noop_sink final : public sink {
 public:
  void write(const log_event&) override {}
  void flush() override {}
};

}  // namespace logspine
