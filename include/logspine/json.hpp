#pragma once

#include <string>
#include <string_view>

#include <logspine/field.hpp>
#include <logspine/log_event.hpp>

namespace logspine {

void append_json_escaped(std::string_view value, std::string& output);

[[nodiscard]] std::string to_json(const field::value_type& value);
[[nodiscard]] std::string to_json(const log_event& event);
[[nodiscard]] std::string to_json_lines_record(const log_event& event);
[[nodiscard]] std::string format_human_readable(const log_event& event);

}  // namespace logspine
