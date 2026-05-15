#pragma once

#include <string>
#include <logspine/log_event.hpp>

namespace logspine {

class formatter {
 public:
  virtual ~formatter() = default;
  virtual void format(const log_event& event, std::string& dest) = 0;
};

}  // namespace logspine
