#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <logspine/field.hpp>

namespace logspine {

class mdc {
 public:
  static void put(std::string_view key, field::value_type value);
  static void remove(std::string_view key);
  static void clear();

  [[nodiscard]] static std::vector<field> get_all();
};

class scoped_mdc {
 public:
  scoped_mdc(std::string_view key, field::value_type value);
  ~scoped_mdc();

  scoped_mdc(const scoped_mdc&) = delete;
  scoped_mdc& operator=(const scoped_mdc&) = delete;

 private:
  std::string key_;
};

}  // namespace logspine
