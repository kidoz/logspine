#include <logspine/sinks/tcp_json_lines_sink.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "net/socket.hpp"
#include "sinks/network_payloads.hpp"

namespace logspine::sinks {

class tcp_json_lines_sink::transport {
public:
  logspine::net::tcp_client client;
};

tcp_json_lines_sink::tcp_json_lines_sink(tcp_json_lines_sink_options options)
    : options_(std::move(options)), transport_(std::make_unique<transport>()) {
  if (options_.port == 0U) {
    throw std::invalid_argument("tcp_json_lines_sink port must be greater than zero");
  }
  if (!options_.lazy_connect) {
    transport_->client.connect(options_.host, options_.port);
  }
}

tcp_json_lines_sink::~tcp_json_lines_sink() = default;

void tcp_json_lines_sink::write(const log_event& event) {
  if (!should_log(event))
    return;

  std::string payload;
  if (formatter_) {
    formatter_->format(event, payload);
  } else {
    payload = detail::make_tcp_json_lines_payload(event);
  }

  std::unique_lock lock(mutex_);
  ++statistics_.write_attempts;

  std::uint32_t retries_remaining = options_.reconnect_on_failure ? options_.max_write_retries : 0U;
  unsigned int backoff_ms = 10;
  for (;;) {
    try {
      ensure_connected();
      transport_->client.send_all(payload);
      last_error_message_.clear();
      return;
    } catch (const std::exception& error) {
      ++statistics_.write_failures;
      record_error_message(error.what());
      if (retries_remaining == 0U) {
        throw;
      }
    } catch (...) {
      ++statistics_.write_failures;
      record_error_message("unknown tcp sink failure");
      if (retries_remaining == 0U) {
        throw;
      }
    }

    --retries_remaining;
    ++statistics_.reconnect_attempts;
    transport_->client.close();

    lock.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
    backoff_ms = std::min(backoff_ms * 2, 1000U);
    lock.lock();
  }
}

void tcp_json_lines_sink::flush() {}

std::uint64_t tcp_json_lines_sink::write_failures() const noexcept {
  std::scoped_lock lock(mutex_);
  return statistics_.write_failures;
}

bool tcp_json_lines_sink::connected() const noexcept {
  std::scoped_lock lock(mutex_);
  return transport_->client.connected();
}

network_sink_statistics tcp_json_lines_sink::statistics() const noexcept {
  std::scoped_lock lock(mutex_);
  return statistics_;
}

std::string tcp_json_lines_sink::last_error_message() const {
  std::scoped_lock lock(mutex_);
  return last_error_message_;
}

void tcp_json_lines_sink::ensure_connected() {
  if (!transport_->client.connected()) {
    transport_->client.connect(options_.host, options_.port);
  }
}

void tcp_json_lines_sink::record_error_message(std::string message) {
  last_error_message_ = std::move(message);
}

} // namespace logspine::sinks
