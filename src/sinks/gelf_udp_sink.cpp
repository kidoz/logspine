#include <logspine/sinks/gelf_udp_sink.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <cstring>
#include <atomic>

#include "net/socket.hpp"
#include "sinks/network_payloads.hpp"
#include "../zlib_helper.hpp"

namespace logspine::sinks {

namespace {

std::uint64_t generate_gelf_message_id() {
  static std::atomic<std::uint64_t> counter{0};
  const auto now = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
  return now ^ (counter.fetch_add(1, std::memory_order_relaxed) << 32);
}

} // namespace

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

#if defined(LOGSPINE_WITH_ZLIB)
  if (options_.compress) {
    std::string compressed;
    if (::logspine::detail::zlib_compress(payload, compressed)) {
      payload = std::move(compressed);
    }
  }
#endif

  std::vector<std::string> chunks;
  if (payload.size() > options_.max_chunk_size) {
    const std::uint64_t message_id = generate_gelf_message_id();
    const std::size_t max_payload_per_chunk = options_.max_chunk_size - 12; // 12 bytes header
    const std::size_t total_chunks = (payload.size() + max_payload_per_chunk - 1) / max_payload_per_chunk;

    if (total_chunks > 128) {
      // Too many chunks for GELF, we drop it or just send what we can. 
      // GELF spec says max 128 chunks. We will truncate payload to 128 chunks.
    }

    const std::size_t chunks_to_send = std::min<std::size_t>(total_chunks, 128);
    for (std::size_t i = 0; i < chunks_to_send; ++i) {
      std::string chunk;
      chunk.reserve(options_.max_chunk_size);
      chunk.push_back('\x1e');
      chunk.push_back('\x0f');
      for (int b = 0; b < 8; ++b) {
        chunk.push_back(static_cast<char>((message_id >> (b * 8)) & 0xFF));
      }
      chunk.push_back(static_cast<char>(i));
      chunk.push_back(static_cast<char>(chunks_to_send));

      std::size_t offset = i * max_payload_per_chunk;
      std::size_t size = std::min(max_payload_per_chunk, payload.size() - offset);
      chunk.append(payload.data() + offset, size);
      chunks.push_back(std::move(chunk));
    }
  } else {
    chunks.push_back(std::move(payload));
  }

  std::unique_lock lock(mutex_);
  ++statistics_.write_attempts;

  std::uint32_t retries_remaining = options_.reconnect_on_failure ? options_.max_write_retries : 0U;
  unsigned int backoff_ms = 10;
  for (;;) {
    try {
      for (const auto& chunk : chunks) {
        transport_->client.send(chunk);
      }
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
