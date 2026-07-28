#include <memory>
#include <vector>

#include <logspine/logspine.hpp>

int main() {
  auto console = std::make_shared<logspine::sinks::console_sink>(
      logspine::sinks::console_sink_options{.format = logspine::sink_format::json_lines});
  auto dispatcher = std::make_shared<logspine::async_dispatcher>(
      std::vector<std::shared_ptr<logspine::sink>>{console},
      logspine::async_options{
          .queue_capacity = 1024, .overflow = logspine::overflow_policy::drop_oldest, .batch_size = 32});

  logspine::logger_registry registry(dispatcher, logspine::level::debug);
  auto log = registry.get("checkout.service");

  log->info("order accepted", {
                                  logspine::kv("order_id", 42),
                                  logspine::kv("customer", "alice"),
                                  logspine::kv("amount", 199.95),
                              });
  LOGSPINE_DEBUG(*log, "cache hit", logspine::kv("key", "user:42"));
  registry.flush();
  return 0;
}
