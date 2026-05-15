#include <logspine/sinks/gelf_udp_sink.hpp>

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

class gelf_udp_sink::transport {
 public:
  logspine::net::udp_client client;
};

gelf_udp_sink::gelf_udp_sink(gelf_udp_sink_options options)
    : options_(std::move(options)), transport_(std::make_unique<transport>()) {
  if (options_.port == 0U) {
    throw std::invalid_argument("gelf_udp_sink port must be greater than zero");
  }
  configure_transport();
}

gelf_udp_sink::~gelf_udp_sink() = default;

void gelf_udp_sink::write(const log_event& event) {
  if (!should_log(event)) return;

  std::string payload;
  if (formatter_) {
    formatter_->format(event, payload);
  } else {
    payload = detail::make_gelf_payload(event, options_.source_host);
  }

  std::unique_lock lock(mutex_);
  ++statistics_.write_attempts;

  std::uint32_t retries_remaining = options_.reconnect_on_failure ? options_.max_write_retries : 0U;
  unsigned int backoff_ms = 10;
  for (;;) {
    try {
      transport_->client.send(payload);
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
      record_error_message("unknown gelf sink failure");
      if (retries_remaining == 0U) {
        throw;
      }
    }

    --retries_remaining;
    ++statistics_.reconnect_attempts;

    lock.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
    backoff_ms = std::min(backoff_ms * 2, 1000U);
    lock.lock();

    configure_transport();
  }
}

void gelf_udp_sink::flush() {}

std::uint64_t gelf_udp_sink::write_failures() const noexcept {
  std::scoped_lock lock(mutex_);
  return statistics_.write_failures;
}

void gelf_udp_sink::configure_transport() {
  transport_->client.configure(options_.host, options_.port);
}

network_sink_statistics gelf_udp_sink::statistics() const noexcept {
  std::scoped_lock lock(mutex_);
  return statistics_;
}

std::string gelf_udp_sink::last_error_message() const {
  std::scoped_lock lock(mutex_);
  return last_error_message_;
}

void gelf_udp_sink::record_error_message(std::string message) { last_error_message_ = std::move(message); }

}  // namespace logspine::sinks
