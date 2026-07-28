#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include <logspine/config.hpp>
#include <logspine/sink.hpp>
#include <logspine/sinks/network_sink_statistics.hpp>

namespace logspine::sinks {

struct otlp_http_sink_options {
  std::string host = "127.0.0.1";
  std::uint16_t port = 4318;
  std::string path = "/v1/logs";
  std::string service_name = "unknown_service";
  bool lazy_connect = true;
  bool reconnect_on_failure = true;
  std::uint32_t max_write_retries = 1;
};

class otlp_http_sink final : public sink {
public:
  explicit otlp_http_sink(otlp_http_sink_options options);
  ~otlp_http_sink() override;

  otlp_http_sink(const otlp_http_sink&) = delete;
  otlp_http_sink& operator=(const otlp_http_sink&) = delete;

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

  otlp_http_sink_options options_;
  std::unique_ptr<transport> transport_;
  mutable std::mutex mutex_;
  network_sink_statistics statistics_;
  std::string last_error_message_;
};

} // namespace logspine::sinks
