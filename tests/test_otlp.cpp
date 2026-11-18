#include <catch2/catch_test_macros.hpp>

#include <logspine/logspine.hpp>

TEST_CASE("OTLP reports disabled networking", "[sinks][otlp]") {
  REQUIRE_FALSE(logspine::network_enabled);
  logspine::sinks::otlp_http_sink sink(logspine::sinks::otlp_http_sink_options{});
  REQUIRE_FALSE(sink.connected());
  REQUIRE_THROWS_AS(sink.write({}), std::runtime_error);
  REQUIRE(sink.statistics().write_attempts == 1);
  REQUIRE(sink.write_failures() == 1);
}
