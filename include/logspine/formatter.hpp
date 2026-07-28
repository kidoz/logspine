#pragma once

#include <logspine/log_event.hpp>
#include <string>

namespace logspine {

class formatter {
public:
  virtual ~formatter() = default;
  virtual void format(const log_event& event, std::string& dest) = 0;
};

} // namespace logspine
