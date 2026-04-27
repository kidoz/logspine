#pragma once

#include <string>
#include <string_view>

#include <logspine/log_event.hpp>

namespace logspine::sinks::detail {

[[nodiscard]] std::string make_tcp_json_lines_payload(const log_event& event);
[[nodiscard]] std::string make_gelf_payload(const log_event& event, std::string_view source_host);

}  // namespace logspine::sinks::detail
