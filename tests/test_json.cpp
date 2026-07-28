#include <cmath>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <logspine/logspine.hpp>

TEST_CASE("JSON escaping handles control characters and quotes", "[json][escaping]") {
  std::string escaped;
  logspine::append_json_escaped("quote\"\nslash\\\t\x01", escaped);
  REQUIRE(escaped == "quote\\\"\\nslash\\\\\\t\\u0001");
}

TEST_CASE("event JSON serialization is deterministic enough for tests", "[json][event]") {
  logspine::log_event event;
  event.severity = logspine::level::warn;
  event.logger_name = "checkout";
  event.message = "escaped \"message\"";
  event.fields = {logspine::kv("customer", "alice"), logspine::kv("amount", 19.95),
                  logspine::kv("invalid", std::nan(""))};

  const auto json = logspine::to_json(event);
  REQUIRE(json.find("\"level\":\"warn\"") != std::string::npos);
  REQUIRE(json.find("\"logger\":\"checkout\"") != std::string::npos);
  REQUIRE(json.find("\"customer\":\"alice\"") != std::string::npos);
  REQUIRE(json.find("\"invalid\":null") != std::string::npos);

  const auto json_line = logspine::to_json_lines_record(event);
  REQUIRE_FALSE(json_line.empty());
  REQUIRE(json_line.back() == '\n');

  const auto human = logspine::format_human_readable(event);
  REQUIRE(human.find("WARN") == std::string::npos);
  REQUIRE(human.find("warn") != std::string::npos);
  REQUIRE(human.find("customer=alice") != std::string::npos);
}
