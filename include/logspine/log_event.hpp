#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <logspine/field.hpp>
#include <logspine/level.hpp>
#include <logspine/source_location.hpp>

namespace logspine {

struct log_event {
  level severity = level::info;
  std::string logger_name;
  std::string message;
  std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
  std::thread::id thread_id = std::this_thread::get_id();
  std::optional<source_location> location;
  std::vector<field> fields;
  std::optional<std::string> trace_id;
  std::optional<std::string> span_id;
  std::optional<std::string> correlation_id;
};

}  // namespace logspine
