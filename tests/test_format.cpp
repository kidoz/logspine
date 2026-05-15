#include <catch2/catch_test_macros.hpp>
#include <logspine/logspine.hpp>
#include <vector>

class test_dispatcher : public logspine::dispatcher {
public:
  std::vector<logspine::log_event> events;
  void dispatch(logspine::log_event event) override { events.push_back(std::move(event)); }
  void flush() override {}
  std::uint64_t dropped_events() const noexcept override { return 0; }
  std::uint64_t sink_failures() const noexcept override { return 0; }
};

TEST_CASE("logger formats strings correctly", "[logger][format]") {
  auto disp = std::make_shared<test_dispatcher>();
  logspine::logger logger("test", disp, logspine::level::info);

  LOGSPINE_INFO(logger, "User {} logged in", 42);
  LOGSPINE_INFO(logger, "Plain string");
  LOGSPINE_INFO(logger, "Fields only", logspine::kv("key", "value"));

  REQUIRE(disp->events.size() == 3);
  REQUIRE(disp->events[0].message == "User 42 logged in");
  REQUIRE(disp->events[1].message == "Plain string");
  REQUIRE(disp->events[2].message == "Fields only");
  REQUIRE(disp->events[2].fields.size() > 0); 
  REQUIRE(disp->events[2].fields.front().key() == "key");
}
