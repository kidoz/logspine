#include <memory>
#include <vector>

#include <logspine/logspine.hpp>

int main() {
  auto bulk_sink = std::make_shared<logspine::sinks::elastic_bulk_file_sink>(
      logspine::sinks::elastic_bulk_file_sink_options{
          .path = "logspine-bulk.ndjson",
          .index_name = "checkout-events",
          .append = false,
      });
  auto dispatcher =
      std::make_shared<logspine::sync_dispatcher>(std::vector<std::shared_ptr<logspine::sink>>{bulk_sink});

  logspine::logger_registry registry(dispatcher, logspine::level::info);
  auto log = registry.get("example.elastic_bulk");

  log->info("order accepted", {
      logspine::kv("order_id", 42),
      logspine::kv("customer", "alice"),
      logspine::kv("amount", 199.95),
  });
  registry.flush();
  return 0;
}
