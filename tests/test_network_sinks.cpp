#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include <logspine/logspine.hpp>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

#if defined(_WIN32)
using native_socket = SOCKET;
constexpr native_socket invalid_socket = INVALID_SOCKET;
#else
using native_socket = int;
constexpr native_socket invalid_socket = -1;
#endif

void close_socket(native_socket socket_handle) noexcept {
#if defined(_WIN32)
  if (socket_handle != INVALID_SOCKET) {
    closesocket(socket_handle);
  }
#else
  if (socket_handle >= 0) {
    close(socket_handle);
  }
#endif
}

class winsock_guard {
public:
  winsock_guard() {
#if defined(_WIN32)
    WSADATA data{};
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
      throw std::runtime_error("WSAStartup failed");
    }
#endif
  }

  ~winsock_guard() {
#if defined(_WIN32)
    WSACleanup();
#endif
  }
};

class tcp_capture_server {
public:
  explicit tcp_capture_server(std::string response = {}) : response_(std::move(response)) {
    listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener_ == invalid_socket) {
      throw std::runtime_error("tcp listener creation failed");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (::bind(listener_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
      close_socket(listener_);
      throw std::runtime_error("tcp bind failed");
    }

    if (::listen(listener_, 1) != 0) {
      close_socket(listener_);
      throw std::runtime_error("tcp listen failed");
    }

    socklen_type address_length = static_cast<socklen_type>(sizeof(address));
    if (::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &address_length) != 0) {
      close_socket(listener_);
      throw std::runtime_error("tcp getsockname failed");
    }

    port_ = ntohs(address.sin_port);
    worker_ = std::thread([this] { run(); });
  }

  ~tcp_capture_server() {
    worker_.join();
    close_socket(listener_);
  }

  [[nodiscard]] std::uint16_t port() const noexcept {
    return port_;
  }

  [[nodiscard]] std::optional<std::string> wait_for_payload(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    ready_.wait_for(lock, timeout, [this] { return payload_.has_value(); });
    return payload_;
  }

private:
#if defined(_WIN32)
  using socklen_type = int;
#else
  using socklen_type = socklen_t;
#endif

  [[nodiscard]] bool complete(std::string_view payload) const {
    if (response_.empty()) {
      return payload.ends_with('\n');
    }
    const auto end_headers = payload.find("\r\n\r\n");
    const auto length = payload.find("\r\nContent-Length: ");
    if (end_headers == std::string_view::npos || length == std::string_view::npos || length >= end_headers) {
      return false;
    }
    constexpr std::string_view prefix = "\r\nContent-Length: ";
    const auto start = payload.data() + length + prefix.size();
    std::size_t body_size{};
    const auto [end, error] = std::from_chars(start, payload.data() + end_headers, body_size);
    return error == std::errc{} && end <= payload.data() + end_headers && payload.size() - end_headers - 4 >= body_size;
  }

  void run() {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(listener_, &readable);
    timeval timeout{2, 0};
#if defined(_WIN32)
    constexpr int descriptor_count = 0;
#else
    const int descriptor_count = listener_ + 1;
#endif
    if (::select(descriptor_count, &readable, nullptr, nullptr, &timeout) <= 0) {
      return;
    }

    const auto client = ::accept(listener_, nullptr, nullptr);
    if (client == invalid_socket) {
      return;
    }
#if defined(_WIN32)
    const DWORD receive_timeout = 2000;
    ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&receive_timeout),
                 sizeof(receive_timeout));
#else
    const timeval receive_timeout{2, 0};
    ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout));
#endif
#if defined(SO_NOSIGPIPE)
    const int no_sigpipe = 1;
    ::setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif

    std::string payload;
    char buffer[512];
    while (true) {
#if defined(_WIN32)
      const auto received = ::recv(client, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
      const auto received = ::recv(client, buffer, sizeof(buffer), 0);
#endif
      if (received <= 0) {
        break;
      }
      payload.append(buffer, static_cast<std::size_t>(received));
      if (complete(payload)) {
#if defined(MSG_NOSIGNAL)
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        std::size_t offset = 0;
        while (offset < response_.size()) {
#if defined(_WIN32)
          const auto sent =
              ::send(client, response_.data() + offset, static_cast<int>(response_.size() - offset), flags);
#else
          const auto sent = ::send(client, response_.data() + offset, response_.size() - offset, flags);
#endif
          if (sent <= 0) {
            break;
          }
          offset += static_cast<std::size_t>(sent);
        }
        std::scoped_lock lock(mutex_);
        payload_ = std::move(payload);
        ready_.notify_all();
        break;
      }
    }
    close_socket(client);
  }

  native_socket listener_ = invalid_socket;
  std::thread worker_;
  std::string response_;
  std::uint16_t port_ = 0;
  std::mutex mutex_;
  std::condition_variable ready_;
  std::optional<std::string> payload_;
};

class udp_capture_server {
public:
  udp_capture_server() {
    socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ == invalid_socket) {
      throw std::runtime_error("udp socket creation failed");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (::bind(socket_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
      close_socket(socket_);
      throw std::runtime_error("udp bind failed");
    }

    socklen_type address_length = static_cast<socklen_type>(sizeof(address));
    if (::getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &address_length) != 0) {
      close_socket(socket_);
      throw std::runtime_error("udp getsockname failed");
    }

    port_ = ntohs(address.sin_port);
    worker_ = std::thread([this] { run(); });
  }

  ~udp_capture_server() {
    close_socket(socket_);
    if (worker_.joinable()) {
      worker_.join();
    }
  }

  [[nodiscard]] std::uint16_t port() const noexcept {
    return port_;
  }

  [[nodiscard]] std::optional<std::string> wait_for_payload(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    ready_.wait_for(lock, timeout, [this] { return payload_.has_value(); });
    return payload_;
  }

private:
#if defined(_WIN32)
  using socklen_type = int;
#else
  using socklen_type = socklen_t;
#endif

  void run() {
    char buffer[2048];
    sockaddr_in peer{};
    socklen_type peer_length = static_cast<socklen_type>(sizeof(peer));
#if defined(_WIN32)
    const int received = ::recvfrom(socket_, buffer, static_cast<int>(sizeof(buffer)), 0,
                                    reinterpret_cast<sockaddr*>(&peer), &peer_length);
#else
    const auto received =
        ::recvfrom(socket_, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&peer), &peer_length);
#endif
    if (received > 0) {
      std::scoped_lock lock(mutex_);
      payload_ = std::string(buffer, buffer + received);
      ready_.notify_all();
    }
  }

  native_socket socket_ = invalid_socket;
  std::thread worker_;
  std::uint16_t port_ = 0;
  std::mutex mutex_;
  std::condition_variable ready_;
  std::optional<std::string> payload_;
};

logspine::log_event make_event() {
  logspine::log_event event;
  event.logger_name = "checkout";
  event.message = "accepted";
  event.severity = logspine::level::info;
  event.fields = {logspine::kv("order_id", 42), logspine::kv("customer", "alice")};
  return event;
}

} // namespace

TEST_CASE("tcp json-lines sink delivers records to a loopback receiver", "[sinks][network][tcp]") {
  winsock_guard winsock;
  tcp_capture_server server;
  logspine::sinks::tcp_json_lines_sink sink({
      .host = "127.0.0.1",
      .port = server.port(),
      .lazy_connect = false,
      .reconnect_on_failure = true,
  });

  REQUIRE(sink.connected());
  sink.write(make_event());
  const auto payload = server.wait_for_payload(std::chrono::milliseconds(1500));
  REQUIRE(payload.has_value());
  REQUIRE(payload->find("\"logger\":\"checkout\"") != std::string::npos);
  REQUIRE(payload->find("\"customer\":\"alice\"") != std::string::npos);
  REQUIRE_FALSE(payload->empty());
  REQUIRE(payload->back() == '\n');
  REQUIRE(sink.write_failures() == 0U);
  REQUIRE(sink.statistics().write_attempts == 1U);
  REQUIRE(sink.statistics().write_failures == 0U);
  REQUIRE(sink.statistics().reconnect_attempts == 0U);
  REQUIRE(sink.last_error_message().empty());
}

TEST_CASE("gelf udp sink delivers records to a loopback receiver", "[sinks][network][udp]") {
  winsock_guard winsock;
  udp_capture_server server;
  logspine::sinks::gelf_udp_sink sink({
      .host = "127.0.0.1",
      .port = server.port(),
      .source_host = "app-host",
      .reconnect_on_failure = true,
  });

  sink.write(make_event());
  const auto payload = server.wait_for_payload(std::chrono::milliseconds(1500));
  REQUIRE(payload.has_value());
  REQUIRE(payload->find("\"version\":\"1.1\"") != std::string::npos);
  REQUIRE(payload->find("\"host\":\"app-host\"") != std::string::npos);
  REQUIRE(payload->find("\"_order_id\":42") != std::string::npos);
  REQUIRE(payload->find("\"_customer\":\"alice\"") != std::string::npos);
  REQUIRE(sink.write_failures() == 0U);
  REQUIRE(sink.statistics().write_attempts == 1U);
  REQUIRE(sink.statistics().write_failures == 0U);
  REQUIRE(sink.statistics().reconnect_attempts == 0U);
  REQUIRE(sink.last_error_message().empty());
}

TEST_CASE("GELF validates chunk size and reports oversized records", "[sinks][network][udp]") {
  REQUIRE_THROWS_AS(logspine::sinks::gelf_udp_sink({.max_chunk_size = 12}), std::invalid_argument);

  logspine::sinks::gelf_udp_sink sink({.max_chunk_size = 32});
  auto event = make_event();
  event.message.assign(3000, 'x');
  REQUIRE_THROWS_AS(sink.write(event), std::length_error);
  const auto stats = sink.statistics();
  REQUIRE(stats.write_attempts == 1);
  REQUIRE(stats.write_failures == 1);
  REQUIRE(stats.reconnect_attempts == 0);
}

TEST_CASE("OTLP checks the collector acknowledgement and preserves attribute types", "[sinks][network][otlp]") {
  [[maybe_unused]] const winsock_guard winsock;
  REQUIRE(logspine::network_enabled);
  std::string status = "200 OK";
  std::string body = "{}";
  std::string expected_error;

  SECTION("accepted") {}
  SECTION("permanently rejected") {
    status = "400 Bad Request";
    expected_error = "HTTP 400";
  }
  SECTION("partial success with a rejected record") {
    body = R"({"partialSuccess":{"rejectedLogRecords":"1","errorMessage":"example"}})";
    expected_error = "rejected 1 log record";
  }
  SECTION("accepted with a collector warning") {
    body = R"({"partialSuccess":{"rejectedLogRecords":0,"errorMessage":"example warning"}})";
  }

  tcp_capture_server server("HTTP/1.1 " + status +
                            "\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: " +
                            std::to_string(body.size()) + "\r\n\r\n" + body);
  logspine::sinks::otlp_http_sink sink({.port = server.port()});
  auto event = make_event();
  event.fields.push_back(logspine::kv("active", true));
  event.fields.push_back(logspine::kv("amount", 12.5));

  if (expected_error.empty()) {
    REQUIRE_NOTHROW(sink.write(event));
    REQUIRE(sink.write_failures() == 0);
    if (body != "{}") {
      REQUIRE(sink.last_error_message() == "example warning");
    }
  } else {
    REQUIRE_THROWS_AS(sink.write(event), std::runtime_error);
    REQUIRE(sink.write_failures() == 1);
    REQUIRE(sink.last_error_message().find(expected_error) != std::string::npos);
  }
  REQUIRE(sink.statistics().write_attempts == 1);
  REQUIRE(sink.statistics().reconnect_attempts == 0);
  const auto request = server.wait_for_payload(std::chrono::seconds(2));
  REQUIRE(request.has_value());
  REQUIRE(request->starts_with("POST /v1/logs HTTP/1.1\r\n"));
  REQUIRE(request->find(R"("key":"order_id","value":{"intValue":"42"})") != std::string::npos);
  REQUIRE(request->find(R"("key":"customer","value":{"stringValue":"alice"})") != std::string::npos);
  REQUIRE(request->find(R"("key":"active","value":{"boolValue":true})") != std::string::npos);
  REQUIRE(request->find(R"("key":"amount","value":{"doubleValue":12.5})") != std::string::npos);
  REQUIRE(request->find(R"("severityNumber":9)") != std::string::npos);
}
