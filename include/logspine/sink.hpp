#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

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

  void set_formatter(std::unique_ptr<formatter> value) {
    std::shared_ptr<formatter> replacement(std::move(value));
    const std::scoped_lock lock(configuration_mutex_);
    formatter_.swap(replacement);
  }

  void set_filter(std::unique_ptr<filter> value) {
    std::shared_ptr<filter> replacement(std::move(value));
    const std::scoped_lock lock(configuration_mutex_);
    filter_.swap(replacement);
  }

  [[nodiscard]] bool should_log(const log_event& event) const {
    std::shared_ptr<filter> current;
    {
      const std::scoped_lock lock(configuration_mutex_);
      current = filter_;
    }
    if (!current) {
      return true;
    }
    const std::scoped_lock lock(callback_mutex_);
    return current->accept(event);
  }

protected:
  [[nodiscard]] bool format_event(const log_event& event, std::string& output) const {
    std::shared_ptr<formatter> current;
    {
      const std::scoped_lock lock(configuration_mutex_);
      current = formatter_;
    }
    if (!current) {
      return false;
    }
    const std::scoped_lock lock(callback_mutex_);
    current->format(event, output);
    return true;
  }

private:
  mutable std::mutex configuration_mutex_;
  mutable std::mutex callback_mutex_;
  std::shared_ptr<formatter> formatter_;
  std::shared_ptr<filter> filter_;
};

} // namespace logspine
