#include <memory>
#include <vector>

#include <logspine/logspine.hpp>

int main() {
  auto console = std::make_shared<logspine::sinks::console_sink>(
      logspine::sinks::console_sink_options{.format = logspine::sink_format::human});
  auto dispatcher =
      std::make_shared<logspine::sync_dispatcher>(std::vector<std::shared_ptr<logspine::sink>>{console});

  logspine::logger_registry registry(dispatcher, logspine::level::info);
  auto log = registry.get("example.basic");
  log->info("service started");
  return 0;
}
