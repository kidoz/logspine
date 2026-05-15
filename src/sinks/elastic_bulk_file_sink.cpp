#include <logspine/sinks/elastic_bulk_file_sink.hpp>

#include <stdexcept>
#include <utility>

#include <logspine/json.hpp>

namespace logspine::sinks {

namespace {

std::string make_action_line(const std::string& index_name) {
  std::string output;
  output.reserve(index_name.size() + 32);
  output += "{\"index\":{\"_index\":\"";
  append_json_escaped(index_name, output);
  output += "\"}}\n";
  return output;
}

}  // namespace

elastic_bulk_file_sink::elastic_bulk_file_sink(elastic_bulk_file_sink_options options) : options_(std::move(options)) {
  auto open_mode = std::ios::out;
  open_mode |= options_.append ? std::ios::app : std::ios::trunc;
  stream_.open(options_.path, open_mode);
  if (!stream_.is_open()) {
    throw std::runtime_error("elastic_bulk_file_sink could not open output file");
  }
}

void elastic_bulk_file_sink::write(const log_event& event) {
  if (!should_log(event)) return;

  std::scoped_lock lock(mutex_);
  stream_ << make_action_line(options_.index_name);
  if (formatter_) {
    std::string buffer;
    formatter_->format(event, buffer);
    stream_ << buffer;
  } else {
    stream_ << to_json_lines_record(event);
  }
}

void elastic_bulk_file_sink::flush() {
  std::scoped_lock lock(mutex_);
  stream_.flush();
}

}  // namespace logspine::sinks
