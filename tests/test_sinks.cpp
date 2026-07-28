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

} // namespace

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
    logspine::sinks::elastic_bulk_file_sink sink({.path = bulk_path, .index_name = "logs-index", .append = false});
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

class custom_formatter : public logspine::formatter {
public:
  void format(const logspine::log_event& event, std::string& dest) override {
    dest = "CUSTOM_FORMAT: " + event.message + "\n";
  }
};

class test_filter : public logspine::filter {
public:
  bool accept(const logspine::log_event& event) override {
    return event.severity >= logspine::level::warn;
  }
};

TEST_CASE("sink supports custom formatter and filter", "[sinks][extensibility]") {
  const auto temp_dir = std::filesystem::temp_directory_path() / "logspine-tests";
  std::filesystem::create_directories(temp_dir);
  const auto file_path = temp_dir / "custom.log";

  {
    logspine::sinks::file_sink sink({.path = file_path, .append = false});
    sink.set_formatter(std::make_unique<custom_formatter>());
    sink.set_filter(std::make_unique<test_filter>());

    logspine::log_event info_event;
    info_event.message = "should be filtered out";
    info_event.severity = logspine::level::info;
    sink.write(info_event);

    logspine::log_event warn_event;
    warn_event.message = "should be logged";
    warn_event.severity = logspine::level::warn;
    sink.write(warn_event);

    sink.flush();
  }

  const auto content = read_all(file_path);
  REQUIRE(content.find("should be filtered out") == std::string::npos);
  REQUIRE(content == "CUSTOM_FORMAT: should be logged\n");
}

TEST_CASE("file sink rotates files when size limit is reached", "[sinks][file][rotation]") {
  const auto temp_dir = std::filesystem::temp_directory_path() / "logspine-tests-rotation";
  std::filesystem::create_directories(temp_dir);
  const auto file_path = temp_dir / "rotation.log";
  std::filesystem::remove_all(temp_dir);
  std::filesystem::create_directories(temp_dir);

  logspine::log_event event;
  event.logger_name = "test";
  event.message = "a very long message that takes up space to trigger rotation quickly";
  event.severity = logspine::level::info;

  {
    logspine::sinks::file_sink sink({.path = file_path,
                                     .format = logspine::sink_format::human,
                                     .append = false,
                                     .max_file_size = 100,
                                     .max_files = 2});

    sink.write(event);
    sink.write(event);
    sink.write(event);
    sink.write(event);
    sink.flush();
  }

  REQUIRE(std::filesystem::exists(file_path));
  REQUIRE(std::filesystem::exists(temp_dir / "rotation.log.1"));
  REQUIRE(std::filesystem::exists(temp_dir / "rotation.log.2"));
  REQUIRE_FALSE(std::filesystem::exists(temp_dir / "rotation.log.3"));
}
