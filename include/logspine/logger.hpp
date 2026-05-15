#pragma once

#include <atomic>
#include <concepts>
#include <format>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <logspine/dispatcher.hpp>
#include <logspine/field.hpp>
#include <logspine/level.hpp>
#include <logspine/source_location.hpp>

namespace logspine {

class logger {
 public:
  logger(std::string name, std::shared_ptr<dispatcher> dispatcher, level minimum_level);

  [[nodiscard]] std::string_view name() const noexcept;
  [[nodiscard]] bool enabled(level value) const noexcept;
  [[nodiscard]] level minimum_level() const noexcept;

  void set_level(level value) noexcept;
  void flush();

  void log_macro_dispatch(level severity, source_location location, std::string_view message) {
    if (enabled(severity)) dispatch(severity, message, {}, location);
  }

  template <typename... Fields>
  requires (sizeof...(Fields) > 0 && (std::is_same_v<std::decay_t<Fields>, field> && ...))
  void log_macro_dispatch(level severity, source_location location, std::string_view message, Fields&&... fields) {
    if (enabled(severity)) {
      std::vector<field> f;
      f.reserve(sizeof...(Fields));
      (f.push_back(std::forward<Fields>(fields)), ...);
      dispatch(severity, message, std::move(f), location);
    }
  }

  template <typename... Args>
  requires (sizeof...(Args) > 0 && !(std::is_same_v<std::decay_t<Args>, field> && ...))
  void log_macro_dispatch(level severity, source_location location, std::format_string<Args...> fmt, Args&&... args) {
    if (enabled(severity)) {
      dispatch(severity, std::format(fmt, std::forward<Args>(args)...), {}, location);
    }
  }

  void log(level severity, std::string_view message,
           source_location location = source_location::current());
  void log(level severity, std::string_view message, std::initializer_list<field> fields,
           source_location location = source_location::current());
  void log(level severity, std::string_view message, std::vector<field> fields,
           source_location location = source_location::current());

  void trace(std::string_view message, source_location location = source_location::current());
  void debug(std::string_view message, source_location location = source_location::current());
  void info(std::string_view message, source_location location = source_location::current());
  void warn(std::string_view message, source_location location = source_location::current());
  void error(std::string_view message, source_location location = source_location::current());
  void fatal(std::string_view message, source_location location = source_location::current());

  void trace(std::string_view message, std::initializer_list<field> fields,
             source_location location = source_location::current());
  void debug(std::string_view message, std::initializer_list<field> fields,
             source_location location = source_location::current());
  void info(std::string_view message, std::initializer_list<field> fields,
            source_location location = source_location::current());
  void warn(std::string_view message, std::initializer_list<field> fields,
            source_location location = source_location::current());
  void error(std::string_view message, std::initializer_list<field> fields,
             source_location location = source_location::current());
  void fatal(std::string_view message, std::initializer_list<field> fields,
             source_location location = source_location::current());

 private:
  void dispatch(level severity, std::string_view message, std::vector<field> fields,
                source_location location);

  std::string name_;
  std::shared_ptr<dispatcher> dispatcher_;
  std::atomic<level> minimum_level_;
};

}  // namespace logspine
