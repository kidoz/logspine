#include <logspine/sinks/file_sink.hpp>

#include <stdexcept>
#include <utility>

#include <logspine/json.hpp>

namespace logspine::sinks {

file_sink::file_sink(file_sink_options options) : options_(std::move(options)) {
  auto open_mode = std::ios::out;
  open_mode |= options_.append ? std::ios::app : std::ios::trunc;
  stream_.open(options_.path, open_mode);
  if (!stream_.is_open()) {
    throw std::runtime_error("file_sink could not open output file");
  }
}

void file_sink::write(const log_event& event) {
  std::scoped_lock lock(mutex_);
  if (options_.format == sink_format::json_lines) {
    stream_ << to_json_lines_record(event);
  } else {
    stream_ << format_human_readable(event) << '\n';
  }
}

void file_sink::flush() {
  std::scoped_lock lock(mutex_);
  stream_.flush();
}

}  // namespace logspine::sinks
