#include <iostream>
#include <memory>
#include <vector>

#include <logspine/logspine.hpp>

int main() {
  std::cerr << "This example sends GELF UDP to 127.0.0.1:12201 and expects a Graylog listener there.\n";

  auto gelf_sink = std::make_shared<logspine::sinks::gelf_udp_sink>(logspine::sinks::gelf_udp_sink_options{
      .host = "127.0.0.1",
      .port = 12201,
      .source_host = "logspine-example",
      .reconnect_on_failure = true,
      .max_write_retries = 1,
  });
  auto dispatcher =
      std::make_shared<logspine::sync_dispatcher>(std::vector<std::shared_ptr<logspine::sink>>{gelf_sink});

  logspine::logger_registry registry(dispatcher, logspine::level::info);
  auto log = registry.get("example.graylog");

  log->info("graylog demo", {
                                logspine::kv("order_id", 42),
                                logspine::kv("customer", "alice"),
                            });
  registry.flush();
  return 0;
}
