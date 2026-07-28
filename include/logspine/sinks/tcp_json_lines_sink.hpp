#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include <logspine/config.hpp>
#include <logspine/sink.hpp>
#include <logspine/sinks/network_sink_statistics.hpp>

namespace logspine::sinks {

struct tcp_json_lines_sink_options {
  std::string host = "127.0.0.1";
  std::uint16_t port = 5000;
  bool lazy_connect = true;
  bool reconnect_on_failure = true;
  std::uint32_t max_write_retries = 1;
};

class tcp_json_lines_sink final : public sink {
public:
  explicit tcp_json_lines_sink(tcp_json_lines_sink_options options);
  ~tcp_json_lines_sink() override;

  tcp_json_lines_sink(const tcp_json_lines_sink&) = delete;
  tcp_json_lines_sink& operator=(const tcp_json_lines_sink&) = delete;

  void write(const log_event& event) override;
  void flush() override;
  [[nodiscard]] std::uint64_t write_failures() const noexcept;
  [[nodiscard]] bool connected() const noexcept;
  [[nodiscard]] network_sink_statistics statistics() const noexcept;
  [[nodiscard]] std::string last_error_message() const;

private:
  class transport;

  void ensure_connected();
  void record_error_message(std::string message);

  tcp_json_lines_sink_options options_;
  std::unique_ptr<transport> transport_;
  mutable std::mutex mutex_;
  network_sink_statistics statistics_;
  std::string last_error_message_;
};

} // namespace logspine::sinks
