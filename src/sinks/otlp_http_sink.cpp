#include <logspine/sinks/otlp_http_sink.hpp>

#include <stdexcept>
#include <utility>

#if LOGSPINE_CONFIG_NETWORK
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>
#include <type_traits>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "net/socket.hpp"
#endif

namespace logspine::sinks {

#if LOGSPINE_CONFIG_NETWORK
namespace {

using nlohmann::json;

json make_attribute_value(const field::value_type& value) {
  return std::visit(
      [](const auto& item) -> json {
        using type = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<type, std::nullptr_t>) {
          return json::object();
        } else if constexpr (std::is_same_v<type, bool>) {
          return {{"boolValue", item}};
        } else if constexpr (std::is_same_v<type, std::string>) {
          return {{"stringValue", item}};
        } else if constexpr (std::is_same_v<type, double>) {
          if (std::isfinite(item)) {
            return {{"doubleValue", item}};
          }
          if (std::isnan(item)) {
            return {{"doubleValue", "NaN"}};
          }
          return {{"doubleValue", item < 0 ? "-Infinity" : "Infinity"}};
        } else {
          if constexpr (std::is_same_v<type, std::uint64_t>) {
            if (item > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
              return {{"stringValue", std::to_string(item)}};
            }
          }
          return {{"intValue", std::to_string(item)}};
        }
      },
      value);
}

std::string make_otlp_json(const log_event& event, const std::string& service_name) {
  auto attributes = json::array();
  for (const auto& entry : event.fields) {
    attributes.push_back({{"key", entry.key()}, {"value", make_attribute_value(entry.value())}});
  }

  const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(event.timestamp.time_since_epoch());
  const auto severity = event.severity <= level::fatal ? 1 + (static_cast<int>(event.severity) * 4) : 0;
  json record = {
      {"timeUnixNano", std::to_string(std::max<std::int64_t>(0, timestamp.count()))},
      {"severityText", to_string(event.severity)},
      {"severityNumber", severity},
      {"body", {{"stringValue", event.message}}},
      {"attributes", std::move(attributes)},
  };
  if (event.trace_id) {
    record["traceId"] = *event.trace_id;
  }
  if (event.span_id) {
    record["spanId"] = *event.span_id;
  }

  const json service = {{"key", "service.name"}, {"value", {{"stringValue", service_name}}}};
  const json scope = {{"scope", json::object()}, {"logRecords", json::array({std::move(record)})}};
  const json resource = {{"resource", {{"attributes", json::array({service})}}}, {"scopeLogs", json::array({scope})}};
  return json{{"resourceLogs", json::array({resource})}}.dump(-1, ' ', false, json::error_handler_t::replace);
}

[[nodiscard]] bool retryable_status(long status) {
  return status == 429 || status == 502 || status == 503 || status == 504;
}

[[nodiscard]] long long rejected_records(const json& partial) {
  auto rejected = partial.find("rejectedLogRecords");
  if (rejected == partial.end()) {
    rejected = partial.find("rejected_log_records");
  }
  if (rejected == partial.end() || rejected->is_null()) {
    return 0;
  }
  if (rejected->is_number_integer() || rejected->is_string()) {
    const auto number = rejected->is_string() ? rejected->get<std::string>() : rejected->dump();
    long long count{};
    const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), count);
    if (error == std::errc{} && end == number.data() + number.size() && count >= 0) {
      return count;
    }
  }
  throw std::runtime_error("invalid OTLP rejected log record count");
}

std::string check_acknowledgement(const std::string& body) {
  const auto response = json::parse(body, nullptr, false);
  if (!response.is_object()) {
    throw std::runtime_error("invalid OTLP JSON response");
  }
  auto partial = response.find("partialSuccess");
  if (partial == response.end()) {
    partial = response.find("partial_success");
  }
  if (partial == response.end() || partial->is_null()) {
    return {};
  }
  if (!partial->is_object()) {
    throw std::runtime_error("invalid OTLP partial success response");
  }
  auto message = partial->find("errorMessage");
  if (message == partial->end()) {
    message = partial->find("error_message");
  }
  const auto detail = message == partial->end() || message->is_null() ? std::string{} : message->get<std::string>();
  const auto count = rejected_records(*partial);
  if (count != 0) {
    throw std::runtime_error("OTLP collector rejected " + std::to_string(count) + " log record(s)" +
                             (detail.empty() ? "" : " (" + detail + ")"));
  }
  return detail;
}

class curl_runtime {
public:
  curl_runtime() {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
      throw std::runtime_error("could not initialize libcurl");
    }
  }
  ~curl_runtime() {
    curl_global_cleanup();
  }
};

struct response_buffer {
  std::string body;
  bool too_large = false;
};

std::size_t append_response(char* data, std::size_t size, std::size_t count, void* context) noexcept {
  constexpr std::size_t max_body_size = 4UL * 1024UL * 1024UL;
  auto& response = *static_cast<response_buffer*>(context);
  const auto bytes = size * count;
  if (bytes > max_body_size - response.body.size()) {
    response.too_large = true;
    return 0;
  }
  try {
    response.body.append(data, bytes);
    return bytes;
  } catch (...) {
    return 0;
  }
}

std::string collector_url(const otlp_http_sink_options& options) {
  if (options.host.empty() ||
      options.host.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._:%[]") !=
          std::string::npos ||
      !options.path.starts_with('/') || options.path.contains('#') ||
      std::ranges::any_of(options.path, [](unsigned char byte) { return byte <= 0x20U || byte == 0x7FU; })) {
    throw std::invalid_argument("invalid OTLP host or path");
  }
  const auto host =
      options.host.contains(':') && !options.host.starts_with('[') ? "[" + options.host + "]" : options.host;
  return "http://" + host + ":" + std::to_string(options.port) + options.path;
}

} // namespace
#endif

class otlp_http_sink::transport {
public:
#if LOGSPINE_CONFIG_NETWORK
  explicit transport(const otlp_http_sink_options& options) {
    static const curl_runtime runtime;
    client_.reset(curl_easy_init());
    if (!client_) {
      throw std::runtime_error("could not create OTLP HTTP client");
    }
    headers_.reset(curl_slist_append(nullptr, "Content-Type: application/json"));
    if (!headers_) {
      throw std::runtime_error("could not create OTLP HTTP headers");
    }

    set_option(CURLOPT_URL, collector_url(options).c_str());
    set_option(CURLOPT_PROXY, "");
    set_option(CURLOPT_HTTP_VERSION, static_cast<long>(CURL_HTTP_VERSION_1_1));
    set_option(CURLOPT_HTTPHEADER, headers_.get());
    set_option(CURLOPT_NOSIGNAL, 1L);
    set_option(CURLOPT_TIMEOUT_MS, 5000L);
    set_option(CURLOPT_WRITEFUNCTION, &append_response);
    if (!options.lazy_connect) {
      auto host = options.host;
      if (host.starts_with('[') && host.ends_with(']')) {
        host = host.substr(1, host.size() - 2);
      }
      eager_connection_.connect(host, options.port);
    }
  }

  [[nodiscard]] bool connected() const noexcept {
    curl_socket_t socket = CURL_SOCKET_BAD;
    return eager_connection_.connected() ||
           (curl_easy_getinfo(client_.get(), CURLINFO_ACTIVESOCKET, &socket) == CURLE_OK && socket != CURL_SOCKET_BAD);
  }

  [[nodiscard]] std::string post(const std::string& payload, bool& retryable) {
    eager_connection_.close();
    response_buffer response;
    set_option(CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload.size()));
    set_option(CURLOPT_POSTFIELDS, payload.c_str());
    set_option(CURLOPT_WRITEDATA, static_cast<void*>(&response));
    const auto result = curl_easy_perform(client_.get());
    set_option(CURLOPT_WRITEDATA, static_cast<void*>(nullptr));
    set_option(CURLOPT_POSTFIELDS, static_cast<const char*>(nullptr));
    if (response.too_large) {
      throw std::runtime_error("OTLP response exceeded the 4 MiB limit");
    }
    if (result != CURLE_OK) {
      retryable = result == CURLE_COULDNT_CONNECT || result == CURLE_COULDNT_RESOLVE_HOST ||
                  result == CURLE_OPERATION_TIMEDOUT || result == CURLE_GOT_NOTHING || result == CURLE_SEND_ERROR ||
                  result == CURLE_RECV_ERROR || result == CURLE_PARTIAL_FILE;
      throw std::runtime_error(curl_easy_strerror(result));
    }
    long status{};
    curl_easy_getinfo(client_.get(), CURLINFO_RESPONSE_CODE, &status);
    if (status != 200) {
      retryable = retryable_status(status);
      throw std::runtime_error("OTLP collector returned HTTP " + std::to_string(status));
    }
    return check_acknowledgement(response.body);
  }

private:
  template <typename T> void set_option(CURLoption option, T value) {
    const auto result = curl_easy_setopt(client_.get(), option, value);
    if (result != CURLE_OK) {
      throw std::runtime_error(curl_easy_strerror(result));
    }
  }

  std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers_{nullptr, curl_slist_free_all};
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> client_{nullptr, curl_easy_cleanup};
  logspine::net::tcp_client eager_connection_;
#else
  explicit transport(const otlp_http_sink_options& options) {
    if (!options.lazy_connect) {
      throw std::runtime_error("network support is disabled");
    }
  }

  [[nodiscard]] bool connected() const noexcept {
    return false;
  }
#endif
};

otlp_http_sink::otlp_http_sink(otlp_http_sink_options options) : options_(std::move(options)) {
  if (options_.port == 0U) {
    throw std::invalid_argument("otlp_http_sink port must be greater than zero");
  }
  transport_ = std::make_unique<transport>(options_);
}

otlp_http_sink::~otlp_http_sink() = default;

void otlp_http_sink::write(const log_event& event) {
  if (!should_log(event)) {
    return;
  }

#if LOGSPINE_CONFIG_NETWORK
  std::string payload;
  if (!format_event(event, payload)) {
    payload = make_otlp_json(event, options_.service_name);
  }

  std::unique_lock lock(mutex_);
  ++statistics_.write_attempts;

  auto retries_remaining = options_.reconnect_on_failure ? options_.max_write_retries : 0U;
  auto backoff = std::chrono::milliseconds(10);
  for (;;) {
    bool retryable = false;
    try {
      last_error_message_ = transport_->post(payload, retryable);
      return;
    } catch (const std::exception& error) {
      ++statistics_.write_failures;
      last_error_message_ = error.what();
      if (!retryable || retries_remaining == 0U) {
        throw;
      }
    }

    --retries_remaining;
    ++statistics_.reconnect_attempts;
    lock.unlock();
    std::this_thread::sleep_for(backoff);
    backoff = std::min(backoff * 2, std::chrono::milliseconds(1000));
    lock.lock();
  }
#else
  const std::scoped_lock lock(mutex_);
  ++statistics_.write_attempts;
  ++statistics_.write_failures;
  last_error_message_ = "network support is disabled";
  throw std::runtime_error(last_error_message_);
#endif
}

void otlp_http_sink::flush() {}

std::uint64_t otlp_http_sink::write_failures() const noexcept {
  std::scoped_lock lock(mutex_);
  return statistics_.write_failures;
}

bool otlp_http_sink::connected() const noexcept {
  std::scoped_lock lock(mutex_);
  return transport_->connected();
}

network_sink_statistics otlp_http_sink::statistics() const noexcept {
  std::scoped_lock lock(mutex_);
  return statistics_;
}

std::string otlp_http_sink::last_error_message() const {
  std::scoped_lock lock(mutex_);
  return last_error_message_;
}

} // namespace logspine::sinks
