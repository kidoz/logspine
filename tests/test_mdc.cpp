#include <catch2/catch_test_macros.hpp>
#include <logspine/mdc.hpp>

TEST_CASE("mdc attaches thread local fields", "[mdc]") {
  logspine::mdc::clear();
  logspine::mdc::put("global_id", "123");

  {
    logspine::scoped_mdc scoped("request_id", "abc-456");
    auto fields = logspine::mdc::get_all();
    REQUIRE(fields.size() == 2);
    REQUIRE(fields[0].key() == "global_id");
    REQUIRE(fields[1].key() == "request_id");
  }

  auto fields = logspine::mdc::get_all();
  REQUIRE(fields.size() == 1);
  REQUIRE(fields[0].key() == "global_id");
}
