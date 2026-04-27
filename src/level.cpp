#include <logspine/level.hpp>

#include <array>
#include <string_view>

namespace logspine {

namespace {

constexpr std::array<std::string_view, 7> kLevelNames = {
    "trace",
    "debug",
    "info",
    "warn",
    "error",
    "fatal",
    "off",
};

}  // namespace

std::string_view to_string(level value) noexcept {
  const auto index = static_cast<std::size_t>(value);
  return index < kLevelNames.size() ? kLevelNames[index] : "unknown";
}

std::optional<level> parse_level(std::string_view value) noexcept {
  for (std::size_t index = 0; index < kLevelNames.size(); ++index) {
    if (kLevelNames[index] == value) {
      return static_cast<level>(index);
    }
  }
  if (value == "warning") {
    return level::warn;
  }
  return std::nullopt;
}

std::uint8_t to_gelf_level(level value) noexcept {
  switch (value) {
    case level::trace:
    case level::debug:
      return 7;
    case level::info:
      return 6;
    case level::warn:
      return 4;
    case level::error:
      return 3;
    case level::fatal:
      return 2;
    case level::off:
      return 1;
  }
  return 7;
}

}  // namespace logspine
