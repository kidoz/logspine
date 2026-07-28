#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include <logspine/sink.hpp>

namespace logspine::sinks {

struct elastic_bulk_file_sink_options {
  std::filesystem::path path;
  std::string index_name = "logspine";
  bool append = true;
};

class elastic_bulk_file_sink final : public sink {
public:
  explicit elastic_bulk_file_sink(elastic_bulk_file_sink_options options);

  void write(const log_event& event) override;
  void flush() override;

private:
  elastic_bulk_file_sink_options options_;
  std::ofstream stream_;
  mutable std::mutex mutex_;
};

} // namespace logspine::sinks
