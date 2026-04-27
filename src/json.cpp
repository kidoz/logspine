#include <logspine/json.hpp>

#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include <logspine/level.hpp>

namespace logspine {

namespace {

void append_json_string(std::string_view value, std::string& output) {
  output.push_back('"');
  append_json_escaped(value, output);
  output.push_back('"');
}

std::string format_timestamp(std::chrono::system_clock::time_point timestamp) {
  const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(timestamp);
  const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - seconds).count();
  const auto raw_time = std::chrono::system_clock::to_time_t(seconds);

  std::tm utc_time{};
#if defined(_WIN32)
  gmtime_s(&utc_time, &raw_time);
#else
  gmtime_r(&raw_time, &utc_time);
#endif

  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ", utc_time.tm_year + 1900,
                utc_time.tm_mon + 1, utc_time.tm_mday, utc_time.tm_hour, utc_time.tm_min, utc_time.tm_sec,
                static_cast<long long>(millis));
  return std::string(buffer);
}

std::string format_thread_id(std::thread::id thread_id) {
  std::ostringstream output;
  output << thread_id;
  return output.str();
}

void append_field_value_json(const field::value_type& value, std::string& output) {
  std::visit(
      [&output](const auto& typed_value) {
        using value_t = std::decay_t<decltype(typed_value)>;
        if constexpr (std::is_same_v<value_t, std::nullptr_t>) {
          output += "null";
        } else if constexpr (std::is_same_v<value_t, bool>) {
          output += typed_value ? "true" : "false";
        } else if constexpr (std::is_same_v<value_t, std::int64_t> || std::is_same_v<value_t, std::uint64_t>) {
          output += std::to_string(typed_value);
        } else if constexpr (std::is_same_v<value_t, double>) {
          if (std::isfinite(typed_value)) {
            char buffer[64];
            const auto [ptr, error] = std::to_chars(buffer, buffer + sizeof(buffer), typed_value);
            if (error == std::errc{}) {
              output.append(buffer, ptr);
            } else {
              output += "null";
            }
          } else {
            output += "null";
          }
        } else if constexpr (std::is_same_v<value_t, std::string>) {
          append_json_string(typed_value, output);
        }
      },
      value);
}

void append_fields_object(const std::vector<field>& fields, std::string& output) {
  output.push_back('{');
  bool first = true;
  for (const auto& entry : fields) {
    if (!first) {
      output.push_back(',');
    }
    first = false;
    append_json_string(entry.key(), output);
    output.push_back(':');
    append_field_value_json(entry.value(), output);
  }
  output.push_back('}');
}

}  // namespace

void append_json_escaped(std::string_view value, std::string& output) {
  static constexpr std::array<char, 16> kHexDigits = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                      '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  for (const char raw_ch : value) {
    const auto ch = static_cast<unsigned char>(raw_ch);
    switch (ch) {
      case '"':
        output += "\\\"";
        break;
      case '\\':
        output += "\\\\";
        break;
      case '\b':
        output += "\\b";
        break;
      case '\f':
        output += "\\f";
        break;
      case '\n':
        output += "\\n";
        break;
      case '\r':
        output += "\\r";
        break;
      case '\t':
        output += "\\t";
        break;
      default:
        if (ch < 0x20U) {
          output += "\\u00";
          output.push_back(kHexDigits[(ch >> 4U) & 0x0FU]);
          output.push_back(kHexDigits[ch & 0x0FU]);
        } else {
          output.push_back(static_cast<char>(ch));
        }
        break;
    }
  }
}

std::string to_json(const field::value_type& value) {
  std::string output;
  append_field_value_json(value, output);
  return output;
}

std::string to_json(const log_event& event) {
  std::string output;
  output.reserve(256);
  output.push_back('{');

  auto append_named_string = [&output](std::string_view key, std::string_view value, bool& first) {
    if (!first) {
      output.push_back(',');
    }
    first = false;
    append_json_string(key, output);
    output.push_back(':');
    append_json_string(value, output);
  };

  auto append_named_object = [&output](std::string_view key, const std::vector<field>& fields, bool& first) {
    if (!first) {
      output.push_back(',');
    }
    first = false;
    append_json_string(key, output);
    output.push_back(':');
    append_fields_object(fields, output);
  };

  bool first = true;
  append_named_string("@timestamp", format_timestamp(event.timestamp), first);
  append_named_string("level", to_string(event.severity), first);
  append_named_string("logger", event.logger_name, first);
  append_named_string("message", event.message, first);
  append_named_string("thread_id", format_thread_id(event.thread_id), first);

  if (event.location.has_value()) {
    append_named_string("file", event.location->file_name(), first);

    if (!first) {
      output.push_back(',');
    }
    first = false;
    append_json_string("line", output);
    output.push_back(':');
    output += std::to_string(event.location->line());

    append_named_string("function", event.location->function_name(), first);
  }

  if (event.trace_id.has_value()) {
    append_named_string("trace_id", *event.trace_id, first);
  }
  if (event.span_id.has_value()) {
    append_named_string("span_id", *event.span_id, first);
  }
  if (event.correlation_id.has_value()) {
    append_named_string("correlation_id", *event.correlation_id, first);
  }

  append_named_object("fields", event.fields, first);

  output.push_back('}');
  return output;
}

std::string to_json_lines_record(const log_event& event) {
  auto output = to_json(event);
  output.push_back('\n');
  return output;
}

std::string format_human_readable(const log_event& event) {
  std::string output;
  output.reserve(256);
  output += format_timestamp(event.timestamp);
  output.push_back(' ');
  output += std::string(to_string(event.severity));
  output.push_back(' ');
  output += event.logger_name;
  output.push_back(' ');
  output += event.message;

  for (const auto& entry : event.fields) {
    output.push_back(' ');
    output += std::string(entry.key());
    output.push_back('=');
    std::visit(
        [&output](const auto& typed_value) {
          using value_t = std::decay_t<decltype(typed_value)>;
          if constexpr (std::is_same_v<value_t, std::nullptr_t>) {
            output += "null";
          } else if constexpr (std::is_same_v<value_t, bool>) {
            output += typed_value ? "true" : "false";
          } else if constexpr (std::is_same_v<value_t, std::int64_t> || std::is_same_v<value_t, std::uint64_t>) {
            output += std::to_string(typed_value);
          } else if constexpr (std::is_same_v<value_t, double>) {
            if (std::isfinite(typed_value)) {
              char buffer[64];
              const auto [ptr, error] = std::to_chars(buffer, buffer + sizeof(buffer), typed_value);
              if (error == std::errc{}) {
                output.append(buffer, ptr);
              } else {
                output += "null";
              }
            } else {
              output += "null";
            }
          } else if constexpr (std::is_same_v<value_t, std::string>) {
            output += typed_value;
          }
        },
        entry.value());
  }

  return output;
}

}  // namespace logspine
