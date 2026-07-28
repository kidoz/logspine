#include "sinks/network_payloads.hpp"

#include <chrono>
#include <cmath>
#include <string>
#include <string_view>

#include <logspine/json.hpp>
#include <logspine/level.hpp>

namespace logspine::sinks::detail {

std::string make_tcp_json_lines_payload(const log_event& event) {
  return to_json_lines_record(event);
}

std::string make_gelf_payload(const log_event& event, std::string_view source_host) {
  std::string output;
  output.reserve(256);
  output += "{\"version\":\"1.1\",\"host\":\"";
  append_json_escaped(source_host, output);
  output += "\",\"short_message\":\"";
  append_json_escaped(event.message, output);
  output += "\",\"timestamp\":";

  const auto seconds = std::chrono::duration<double>(event.timestamp.time_since_epoch()).count();
  if (std::isfinite(seconds)) {
    output += std::to_string(seconds);
  } else {
    output += "0.0";
  }

  output += ",\"level\":";
  output += std::to_string(to_gelf_level(event.severity));
  output += ",\"_logger\":\"";
  append_json_escaped(event.logger_name, output);
  output.push_back('"');

  for (const auto& entry : event.fields) {
    output += ",\"_";
    append_json_escaped(entry.key(), output);
    output += "\":";
    output += to_json(entry.value());
  }

  output.push_back('}');
  return output;
}

} // namespace logspine::sinks::detail
