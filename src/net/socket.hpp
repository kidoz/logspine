#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace logspine::net {

class tcp_client {
 public:
  tcp_client();
  ~tcp_client();

  tcp_client(const tcp_client&) = delete;
  tcp_client& operator=(const tcp_client&) = delete;

  void connect(const std::string& host, std::uint16_t port);
  void send_all(std::string_view payload);
  [[nodiscard]] bool connected() const noexcept;
  void close() noexcept;

 private:
  [[maybe_unused]] bool connected_ = false;
#if defined(_WIN32)
  [[maybe_unused]] std::uintptr_t socket_handle_ = static_cast<std::uintptr_t>(~0ULL);
#else
  [[maybe_unused]] int socket_handle_ = -1;
#endif
};

class udp_client {
 public:
  udp_client();
  ~udp_client();

  udp_client(const udp_client&) = delete;
  udp_client& operator=(const udp_client&) = delete;

  void configure(const std::string& host, std::uint16_t port);
  void send(std::string_view payload);
  [[nodiscard]] bool configured() const noexcept;
  void close() noexcept;

 private:
#if defined(_WIN32)
  [[maybe_unused]] std::uintptr_t socket_handle_ = static_cast<std::uintptr_t>(~0ULL);
#else
  [[maybe_unused]] int socket_handle_ = -1;
#endif
  [[maybe_unused]] std::vector<unsigned char> destination_;
};

}  // namespace logspine::net
