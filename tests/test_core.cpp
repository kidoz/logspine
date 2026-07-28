#include <atomic>
#include <memory>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <logspine/logspine.hpp>

namespace {

class recording_sink final : public logspine::sink {
public:
  void write(const logspine::log_event& event) override {
    events.push_back(event);
  }
  void flush() override {
    ++flush_count;
  }

  std::vector<logspine::log_event> events;
  std::size_t flush_count = 0;
};

logspine::field build_counted_field(std::atomic<int>& counter) {
  ++counter;
  return logspine::kv("counted", 1);
}

} // namespace

TEST_CASE("level helpers expose text and GELF mappings", "[core][level]") {
  REQUIRE(logspine::to_string(logspine::level::info) == "info");
  REQUIRE(logspine::parse_level("warn").value() == logspine::level::warn);
  REQUIRE_FALSE(logspine::parse_level("missing").has_value());
  REQUIRE(logspine::to_gelf_level(logspine::level::error) == 3U);
}

TEST_CASE("registry returns stable logger instances and dispatches fields", "[core][registry]") {
  auto sink = std::make_shared<recording_sink>();
  auto dispatcher = std::make_shared<logspine::sync_dispatcher>(std::vector<std::shared_ptr<logspine::sink>>{sink});
  logspine::logger_registry registry(dispatcher, logspine::level::info);

  auto logger_a = registry.get("checkout");
  auto logger_b = registry.get("checkout");
  REQUIRE(logger_a == logger_b);

  logger_a->info("accepted", {logspine::kv("order_id", 42), logspine::kv("customer", "alice")});
  REQUIRE(sink->events.size() == 1U);
  REQUIRE(sink->events.front().logger_name == "checkout");
  REQUIRE(sink->events.front().fields.size() == 2U);
}

TEST_CASE("disabled macro paths do not build fields", "[core][macros]") {
  auto sink = std::make_shared<recording_sink>();
  auto dispatcher = std::make_shared<logspine::sync_dispatcher>(std::vector<std::shared_ptr<logspine::sink>>{sink});
  logspine::logger_registry registry(dispatcher, logspine::level::info);
  auto logger = registry.get("checkout");

  std::atomic<int> field_builds{0};
  LOGSPINE_DEBUG(*logger, "suppressed", build_counted_field(field_builds));
  REQUIRE(field_builds.load() == 0);
  REQUIRE(sink->events.empty());
}

TEST_CASE("registry level changes enable debug logging and flush sinks", "[core][levels]") {
  auto sink = std::make_shared<recording_sink>();
  auto dispatcher = std::make_shared<logspine::sync_dispatcher>(std::vector<std::shared_ptr<logspine::sink>>{sink});
  logspine::logger_registry registry(dispatcher, logspine::level::info);
  auto logger = registry.get("checkout");

  registry.set_level(logspine::level::debug);
  LOGSPINE_DEBUG(*logger, "emitted", logspine::kv("hits", 3));
  REQUIRE(sink->events.size() == 1U);
  REQUIRE(sink->events.back().message == "emitted");

  logger->flush();
  REQUIRE(sink->flush_count == 1U);
}
