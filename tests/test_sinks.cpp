#include "sinks/network_payloads.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <logspine/logspine.hpp>

namespace {

std::string read_all(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("file and elastic bulk sinks emit expected record shapes", "[sinks][file]") {
  const auto temp_dir = std::filesystem::temp_directory_path() / "logspine-tests";
  std::filesystem::create_directories(temp_dir);

  logspine::log_event event;
  event.logger_name = "checkout";
  event.message = "accepted";
  event.severity = logspine::level::info;
  event.fields = {logspine::kv("order_id", 42)};

  const auto file_path = temp_dir / "events.jsonl";
  {
    logspine::sinks::file_sink sink({.path = file_path, .format = logspine::sink_format::json_lines, .append = false});
    sink.write(event);
    sink.flush();
  }

  const auto file_content = read_all(file_path);
  REQUIRE(file_content.find("\"logger\":\"checkout\"") != std::string::npos);
  REQUIRE_FALSE(file_content.empty());
  REQUIRE(file_content.back() == '\n');

  const auto bulk_path = temp_dir / "bulk.ndjson";
  {
    logspine::sinks::elastic_bulk_file_sink sink(
        {.path = bulk_path, .index_name = "logs-index", .append = false});
    sink.write(event);
    sink.flush();
  }

  const auto bulk_content = read_all(bulk_path);
  REQUIRE(bulk_content.find("{\"index\":{\"_index\":\"logs-index\"}}") != std::string::npos);
  REQUIRE(bulk_content.find("\"message\":\"accepted\"") != std::string::npos);
}

TEST_CASE("network payload helpers keep deterministic framing", "[sinks][payloads]") {
  logspine::log_event event;
  event.logger_name = "checkout";
  event.message = "accepted";
  event.severity = logspine::level::info;
  event.fields = {logspine::kv("order_id", 42)};

  const auto tcp_payload = logspine::sinks::detail::make_tcp_json_lines_payload(event);
  REQUIRE_FALSE(tcp_payload.empty());
  REQUIRE(tcp_payload.back() == '\n');
  REQUIRE(tcp_payload.find("\"logger\":\"checkout\"") != std::string::npos);

  const auto gelf_payload = logspine::sinks::detail::make_gelf_payload(event, "app-host");
  REQUIRE(gelf_payload.find("\"version\":\"1.1\"") != std::string::npos);
  REQUIRE(gelf_payload.find("\"host\":\"app-host\"") != std::string::npos);
  REQUIRE(gelf_payload.find("\"short_message\":\"accepted\"") != std::string::npos);
  REQUIRE(gelf_payload.find("\"level\":6") != std::string::npos);
  REQUIRE(gelf_payload.find("\"_logger\":\"checkout\"") != std::string::npos);
  REQUIRE(gelf_payload.find("\"_order_id\":42") != std::string::npos);
}
