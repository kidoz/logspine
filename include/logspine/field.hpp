#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace logspine {

class field {
 public:
  using value_type = std::variant<std::nullptr_t, bool, std::int64_t, std::uint64_t, double, std::string>;

  field(std::string key, value_type value);

  [[nodiscard]] std::string_view key() const noexcept;
  [[nodiscard]] const value_type& value() const noexcept;

 private:
  std::string key_;
  value_type value_;
};

[[nodiscard]] inline field kv(std::string_view key, std::nullptr_t) {
  return field(std::string(key), nullptr);
}

[[nodiscard]] inline field kv(std::string_view key, bool value) {
  return field(std::string(key), value);
}

[[nodiscard]] inline field kv(std::string_view key, const char* value) {
  return field(std::string(key), value == nullptr ? field::value_type{nullptr} : field::value_type{std::string(value)});
}

[[nodiscard]] inline field kv(std::string_view key, std::string_view value) {
  return field(std::string(key), std::string(value));
}

[[nodiscard]] inline field kv(std::string_view key, const std::string& value) {
  return field(std::string(key), value);
}

template <typename T>
concept signed_integral_not_bool = std::signed_integral<T> && (!std::same_as<std::remove_cvref_t<T>, bool>);

template <typename T>
concept unsigned_integral_not_bool =
    std::unsigned_integral<T> && (!std::same_as<std::remove_cvref_t<T>, bool>) &&
    (!std::same_as<std::remove_cvref_t<T>, char>) && (!std::same_as<std::remove_cvref_t<T>, signed char>) &&
    (!std::same_as<std::remove_cvref_t<T>, unsigned char>);

template <signed_integral_not_bool T>
[[nodiscard]] inline field kv(std::string_view key, T value) {
  return field(std::string(key), static_cast<std::int64_t>(value));
}

template <unsigned_integral_not_bool T>
[[nodiscard]] inline field kv(std::string_view key, T value) {
  if constexpr (sizeof(T) > sizeof(std::uint64_t)) {
    static_assert(sizeof(T) <= sizeof(std::uint64_t), "Unsupported integer width");
  }
  return field(std::string(key), static_cast<std::uint64_t>(value));
}

template <std::floating_point T>
[[nodiscard]] inline field kv(std::string_view key, T value) {
  return field(std::string(key), static_cast<double>(value));
}

}  // namespace logspine
