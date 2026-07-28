#include "net/socket.hpp"

#include <logspine/config.hpp>

#include <stdexcept>

#if LOGSPINE_CONFIG_NETWORK

#include <cstring>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace logspine::net {

namespace {

class winsock_runtime {
public:
  winsock_runtime() {
#if defined(_WIN32)
    WSADATA data{};
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
      throw std::runtime_error("WSAStartup failed");
    }
#endif
  }

  ~winsock_runtime() {
#if defined(_WIN32)
    WSACleanup();
#endif
  }
};

winsock_runtime& socket_runtime() {
  static winsock_runtime runtime;
  return runtime;
}

using native_socket =
#if defined(_WIN32)
    SOCKET;
#else
    int;
#endif

constexpr native_socket invalid_socket =
#if defined(_WIN32)
    INVALID_SOCKET;
#else
    -1;
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

void set_socket_timeout(native_socket socket_handle, int timeout_ms) noexcept {
#if defined(_WIN32)
  DWORD timeout = timeout_ms;
  setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
  struct timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const void*>(&tv),
             static_cast<socklen_t>(sizeof(tv)));
#endif
}

[[nodiscard]] bool last_error_is_interrupted() noexcept {
#if defined(_WIN32)
  return WSAGetLastError() == WSAEINTR;
#else
  return errno == EINTR;
#endif
}

native_socket connect_tcp_socket(const std::string& host, std::uint16_t port) {
  socket_runtime();

  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  addrinfo* result = nullptr;
  const auto service = std::to_string(port);
  if (getaddrinfo(host.c_str(), service.c_str(), &hints, &result) != 0) {
    throw std::runtime_error("getaddrinfo failed");
  }

  native_socket socket_handle = invalid_socket;
  for (addrinfo* current = result; current != nullptr; current = current->ai_next) {
    socket_handle =
        static_cast<native_socket>(::socket(current->ai_family, current->ai_socktype, current->ai_protocol));
    if (socket_handle == invalid_socket) {
      continue;
    }
    set_socket_timeout(socket_handle, 5000); // 5 seconds timeout
#if defined(_WIN32)
    const auto address_length = static_cast<int>(current->ai_addrlen);
#else
    const auto address_length = current->ai_addrlen;
#endif
    if (::connect(socket_handle, current->ai_addr, address_length) == 0) {
      freeaddrinfo(result);
      return socket_handle;
    }
    close_socket(socket_handle);
    socket_handle = invalid_socket;
  }

  freeaddrinfo(result);
  throw std::runtime_error("tcp connect failed");
}

std::pair<native_socket, std::vector<unsigned char>> configure_udp_socket(const std::string& host, std::uint16_t port) {
  socket_runtime();

  addrinfo hints{};
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_family = AF_UNSPEC;

  addrinfo* result = nullptr;
  const auto service = std::to_string(port);
  if (getaddrinfo(host.c_str(), service.c_str(), &hints, &result) != 0) {
    throw std::runtime_error("getaddrinfo failed");
  }

  for (addrinfo* current = result; current != nullptr; current = current->ai_next) {
    const auto socket_handle =
        static_cast<native_socket>(::socket(current->ai_family, current->ai_socktype, current->ai_protocol));
    if (socket_handle == invalid_socket) {
      continue;
    }
    set_socket_timeout(socket_handle, 5000); // 5 seconds timeout

    std::vector<unsigned char> destination(current->ai_addrlen);
    std::memcpy(destination.data(), current->ai_addr, current->ai_addrlen);
    freeaddrinfo(result);
    return {socket_handle, std::move(destination)};
  }

  freeaddrinfo(result);
  throw std::runtime_error("udp socket creation failed");
}

} // namespace

tcp_client::tcp_client() = default;
tcp_client::~tcp_client() {
  close();
}

void tcp_client::connect(const std::string& host, std::uint16_t port) {
  close();
  const auto connected_socket = connect_tcp_socket(host, port);
  socket_handle_ = static_cast<decltype(socket_handle_)>(connected_socket);
  connected_ = true;
}

void tcp_client::send_all(std::string_view payload) {
  if (!connected_) {
    throw std::runtime_error("tcp socket is not connected");
  }

  std::size_t offset = 0;
  while (offset < payload.size()) {
#if defined(_WIN32)
    const int sent = ::send(static_cast<SOCKET>(socket_handle_), payload.data() + offset,
                            static_cast<int>(payload.size() - offset), 0);
#else
    const auto sent = ::send(socket_handle_, payload.data() + offset, payload.size() - offset, 0);
#endif
    if (sent < 0 && last_error_is_interrupted()) {
      continue;
    }
    if (sent <= 0) {
      close();
      throw std::runtime_error("tcp send failed");
    }
    offset += static_cast<std::size_t>(sent);
  }
}

bool tcp_client::connected() const noexcept {
  return connected_;
}

void tcp_client::close() noexcept {
  if (!connected_) {
    return;
  }

  close_socket(static_cast<native_socket>(socket_handle_));
  socket_handle_ = static_cast<decltype(socket_handle_)>(invalid_socket);
  connected_ = false;
}

udp_client::udp_client() = default;
udp_client::~udp_client() {
  close();
}

void udp_client::configure(const std::string& host, std::uint16_t port) {
  close();
  auto [socket_handle, destination] = configure_udp_socket(host, port);
  socket_handle_ = static_cast<decltype(socket_handle_)>(socket_handle);
  destination_ = std::move(destination);
}

void udp_client::send(std::string_view payload) {
  if (destination_.empty()) {
    throw std::runtime_error("udp socket is not configured");
  }

  for (;;) {
#if defined(_WIN32)
    const auto sent =
        ::sendto(static_cast<SOCKET>(socket_handle_), payload.data(), static_cast<int>(payload.size()), 0,
                 reinterpret_cast<const sockaddr*>(destination_.data()), static_cast<int>(destination_.size()));
#else
    const auto destination_size = static_cast<socklen_t>(destination_.size());
    const auto sent = ::sendto(socket_handle_, payload.data(), payload.size(), 0,
                               reinterpret_cast<const sockaddr*>(destination_.data()), destination_size);
#endif
    if (sent < 0 && last_error_is_interrupted()) {
      continue;
    }
    if (sent < 0) {
      close();
      throw std::runtime_error("udp send failed");
    }
    break;
  }
}

bool udp_client::configured() const noexcept {
  return !destination_.empty();
}

void udp_client::close() noexcept {
  if (destination_.empty()) {
    return;
  }

  close_socket(static_cast<native_socket>(socket_handle_));
  destination_.clear();
  socket_handle_ = static_cast<decltype(socket_handle_)>(invalid_socket);
}

} // namespace logspine::net

#else

namespace logspine::net {

tcp_client::tcp_client() = default;
tcp_client::~tcp_client() = default;
void tcp_client::connect(const std::string&, std::uint16_t) {
  throw std::runtime_error("network support is disabled");
}
void tcp_client::send_all(std::string_view) {
  throw std::runtime_error("network support is disabled");
}
bool tcp_client::connected() const noexcept {
  return false;
}
void tcp_client::close() noexcept {}

udp_client::udp_client() = default;
udp_client::~udp_client() = default;
void udp_client::configure(const std::string&, std::uint16_t) {
  throw std::runtime_error("network support is disabled");
}
void udp_client::send(std::string_view) {
  throw std::runtime_error("network support is disabled");
}
void udp_client::close() noexcept {}

} // namespace logspine::net

#endif
