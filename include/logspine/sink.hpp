#pragma once

#include <cstdint>
#include <memory>

#include <logspine/filter.hpp>
#include <logspine/formatter.hpp>
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

  void set_formatter(std::unique_ptr<formatter> f) {
    formatter_ = std::move(f);
  }
  void set_filter(std::unique_ptr<filter> f) {
    filter_ = std::move(f);
  }

  [[nodiscard]] bool should_log(const log_event& event) const {
    return filter_ ? filter_->accept(event) : true;
  }

protected:
  std::unique_ptr<formatter> formatter_;
  std::unique_ptr<filter> filter_;
};

} // namespace logspine
