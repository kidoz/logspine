#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>

#include <logspine/sink.hpp>

namespace logspine::sinks {

struct file_sink_options {
  std::filesystem::path path;
  sink_format format = sink_format::json_lines;
  bool append = true;
  std::size_t max_file_size = 0; // 0 means no rotation
  std::uint32_t max_files = 0;
  bool compress_rotated = false;
};

class file_sink final : public sink {
 public:
  explicit file_sink(file_sink_options options);

  void write(const log_event& event) override;
  void flush() override;

 private:
  void rotate_if_needed();
  void open_file();

  file_sink_options options_;
  std::ofstream stream_;
  mutable std::mutex mutex_;
};

}  // namespace logspine::sinks
