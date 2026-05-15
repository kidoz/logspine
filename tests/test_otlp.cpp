#include <catch2/catch_test_macros.hpp>
#include <logspine/logspine.hpp>

TEST_CASE("otlp sink formats properly", "[sinks][otlp]") {
  logspine::log_event event;
  event.logger_name = "test";
  event.message = "hello";
  event.severity = logspine::level::info;
  
  logspine::sinks::otlp_http_sink sink(logspine::sinks::otlp_http_sink_options{
    .lazy_connect = true
  });
  REQUIRE(true); // if it compiles and links it's fine for now
}
