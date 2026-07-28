#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include <logspine/config.hpp>
#include <logspine/sink.hpp>
#include <logspine/sinks/network_sink_statistics.hpp>

namespace logspine::sinks {

struct gelf_udp_sink_options {
  std::string host = "127.0.0.1";
  std::uint16_t port = 12201;
  std::string source_host = "localhost";
  bool reconnect_on_failure = true;
  std::uint32_t max_write_retries = 1;
  bool compress = false;             // Requires zlib support at compile time
  std::size_t max_chunk_size = 1024; // Defaults to 1024 for safe UDP MTU
};

class gelf_udp_sink final : public sink {
public:
  explicit gelf_udp_sink(gelf_udp_sink_options options);
  ~gelf_udp_sink() override;

  gelf_udp_sink(const gelf_udp_sink&) = delete;
  gelf_udp_sink& operator=(const gelf_udp_sink&) = delete;

  void write(const log_event& event) override;
  void flush() override;
  [[nodiscard]] std::uint64_t write_failures() const noexcept;
  [[nodiscard]] network_sink_statistics statistics() const noexcept;
  [[nodiscard]] std::string last_error_message() const;

private:
  class transport;

  void configure_transport();
  void record_error_message(std::string message);

  gelf_udp_sink_options options_;
  std::unique_ptr<transport> transport_;
  mutable std::mutex mutex_;
  network_sink_statistics statistics_;
  std::string last_error_message_;
};

} // namespace logspine::sinks
