#include <atomic>
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
  tcp_capture_server() {
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
    stop_requested_.store(true, std::memory_order_relaxed);
    close_socket(listener_);
    if (client_ != invalid_socket) {
      close_socket(client_);
    }
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
    sockaddr_in peer{};
    socklen_type peer_length = static_cast<socklen_type>(sizeof(peer));
    client_ = ::accept(listener_, reinterpret_cast<sockaddr*>(&peer), &peer_length);
    if (client_ == invalid_socket) {
      return;
    }

    std::string payload;
    char buffer[512];
    while (!stop_requested_.load(std::memory_order_relaxed)) {
#if defined(_WIN32)
      const int received = ::recv(client_, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
      const auto received = ::recv(client_, buffer, sizeof(buffer), 0);
#endif
      if (received <= 0) {
        break;
      }
      payload.append(buffer, buffer + received);
      if (!payload.empty() && payload.back() == '\n') {
        std::scoped_lock lock(mutex_);
        payload_ = std::move(payload);
        ready_.notify_all();
        return;
      }
    }
  }

  native_socket listener_ = invalid_socket;
  native_socket client_ = invalid_socket;
  std::thread worker_;
  std::atomic<bool> stop_requested_{false};
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
