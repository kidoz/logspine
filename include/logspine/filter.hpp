#pragma once

#include <logspine/log_event.hpp>

namespace logspine {

class filter {
public:
  virtual ~filter() = default;
  virtual bool accept(const log_event& event) = 0;
};

} // namespace logspine
