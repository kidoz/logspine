#include <logspine/sinks/console_sink.hpp>

#include <iostream>

#include <logspine/json.hpp>

namespace logspine::sinks {

console_sink::console_sink(console_sink_options options) : options_(options) {}

void console_sink::write(const log_event& event) {
  std::scoped_lock lock(mutex_);
  auto& stream = select_stream(event.severity);
  if (options_.format == sink_format::json_lines) {
    stream << to_json_lines_record(event);
  } else {
    stream << format_human_readable(event) << '\n';
  }
}

void console_sink::flush() {
  std::scoped_lock lock(mutex_);
  std::cout.flush();
  std::cerr.flush();
}

std::ostream& console_sink::select_stream(level severity) const noexcept {
  if (options_.use_stderr_for_warnings && severity >= level::warn && severity != level::off) {
    return std::cerr;
  }
  return std::cout;
}

}  // namespace logspine::sinks
