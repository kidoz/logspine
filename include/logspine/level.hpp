#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace logspine {

enum class level : std::uint8_t {
  trace = 0,
  debug = 1,
  info = 2,
  warn = 3,
  error = 4,
  fatal = 5,
  off = 6,
};

[[nodiscard]] std::string_view to_string(level value) noexcept;
[[nodiscard]] std::optional<level> parse_level(std::string_view value) noexcept;
[[nodiscard]] std::uint8_t to_gelf_level(level value) noexcept;

}  // namespace logspine
