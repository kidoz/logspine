#include <logspine/field.hpp>

namespace logspine {

field::field(std::string key, value_type value) : key_(std::move(key)), value_(std::move(value)) {}

std::string_view field::key() const noexcept { return key_; }

const field::value_type& field::value() const noexcept { return value_; }

}  // namespace logspine
