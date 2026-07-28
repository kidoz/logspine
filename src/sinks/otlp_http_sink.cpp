#include <logspine/sinks/otlp_http_sink.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "net/socket.hpp"
#include <logspine/json.hpp>

namespace logspine::sinks {

namespace {

std::string make_otlp_json(const log_event& event, const std::string& service_name) {
  std::string output;
  output.reserve(1024);

  // Minimal OTLP JSON structure for logs
  output += "{\"resourceLogs\":[{\"resource\":{\"attributes\":[{\"key\":\"service.name\",\"value\":{\"stringValue\":\"";
  append_json_escaped(service_name, output);
  output += "\"}}]},\"scopeLogs\":[{\"scope\":{},\"logRecords\":[{";

  bool first = true;
  auto add_string = [&](const char* key, std::string_view val) {
    if (!first)
      output += ",";
    first = false;
    output += "\"";
    output += key;
    output += "\":\"";
    append_json_escaped(val, output);
    output += "\"";
  };

  const auto seconds = std::chrono::time_point_cast<std::chrono::nanoseconds>(event.timestamp);
  auto nanos = seconds.time_since_epoch().count();

  if (!first)
    output += ",";
  first = false;
  output += "\"timeUnixNano\":\"" + std::to_string(nanos) + "\"";

  add_string("severityText", to_string(event.severity));

  if (!first)
    output += ",";
  first = false;
  output += "\"severityNumber\":" + std::to_string(static_cast<int>(event.severity) * 4);

  if (event.trace_id) {
    add_string("traceId", *event.trace_id);
  }
  if (event.span_id) {
    add_string("spanId", *event.span_id);
  }

  // body
  if (!first)
    output += ",";
  first = false;
  output += "\"body\":{\"stringValue\":\"";
  append_json_escaped(event.message, output);
  output += "\"}";

  // attributes
  if (!event.fields.empty()) {
    if (!first)
      output += ",";
    first = false;
    output += "\"attributes\":[";
    bool first_attr = true;
    for (const auto& field : event.fields) {
      if (!first_attr)
        output += ",";
      first_attr = false;
      output += "{\"key\":\"";
      append_json_escaped(field.key(), output);
      output += "\",\"value\":{\"stringValue\":\"";
      append_json_escaped(to_json(field.value()), output);
      output += "\"}}";
    }
    output += "]";
  }

  output += "}]}]}]}";
  return output;
}

std::string make_http_post(const std::string& host, const std::string& path, const std::string& body) {
  std::string req = "POST " + path + " HTTP/1.1\r\n";
  req += "Host: " + host + "\r\n";
  req += "Content-Type: application/json\r\n";
  req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  req += "Connection: keep-alive\r\n";
  req += "\r\n";
  req += body;
  return req;
}

} // namespace

class otlp_http_sink::transport {
public:
  logspine::net::tcp_client client;
};

otlp_http_sink::otlp_http_sink(otlp_http_sink_options options)
    : options_(std::move(options)), transport_(std::make_unique<transport>()) {
  if (options_.port == 0U) {
    throw std::invalid_argument("otlp_http_sink port must be greater than zero");
  }
  if (!options_.lazy_connect) {
    transport_->client.connect(options_.host, options_.port);
  }
}

otlp_http_sink::~otlp_http_sink() = default;

void otlp_http_sink::write(const log_event& event) {
  if (!should_log(event))
    return;

  std::string payload;
  if (formatter_) {
    formatter_->format(event, payload);
  } else {
    payload = make_otlp_json(event, options_.service_name);
  }

  std::string http_request = make_http_post(options_.host, options_.path, payload);

  std::unique_lock lock(mutex_);
  ++statistics_.write_attempts;

  std::uint32_t retries_remaining = options_.reconnect_on_failure ? options_.max_write_retries : 0U;
  unsigned int backoff_ms = 10;
  for (;;) {
    try {
      ensure_connected();
      transport_->client.send_all(http_request);
      // Note: A true HTTP client would read the response here.
      // For simplicity in this socket transport, we just push the data.
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
      record_error_message("unknown otlp sink failure");
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

void otlp_http_sink::flush() {}

std::uint64_t otlp_http_sink::write_failures() const noexcept {
  std::scoped_lock lock(mutex_);
  return statistics_.write_failures;
}

bool otlp_http_sink::connected() const noexcept {
  std::scoped_lock lock(mutex_);
  return transport_->client.connected();
}

network_sink_statistics otlp_http_sink::statistics() const noexcept {
  std::scoped_lock lock(mutex_);
  return statistics_;
}

std::string otlp_http_sink::last_error_message() const {
  std::scoped_lock lock(mutex_);
  return last_error_message_;
}

void otlp_http_sink::ensure_connected() {
  if (!transport_->client.connected()) {
    transport_->client.connect(options_.host, options_.port);
  }
}

void otlp_http_sink::record_error_message(std::string message) {
  last_error_message_ = std::move(message);
}

} // namespace logspine::sinks
