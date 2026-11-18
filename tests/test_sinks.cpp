#include "sinks/network_payloads.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <latch>
#include <memory>
#include <random>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include <logspine/logspine.hpp>

namespace {

class temp_directory {
public:
  temp_directory() {
    std::random_device random;
    do {
      path = std::filesystem::temp_directory_path() / ("logspine-tests-" + std::to_string(random()));
    } while (!std::filesystem::create_directory(path));
  }

  ~temp_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }

  temp_directory(const temp_directory&) = delete;
  temp_directory& operator=(const temp_directory&) = delete;

  std::filesystem::path path;
};

std::string read_all(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("file and elastic bulk sinks emit expected record shapes", "[sinks][file]") {
  const temp_directory temp_dir;

  logspine::log_event event;
  event.logger_name = "checkout";
  event.message = "accepted";
  event.severity = logspine::level::info;
  event.fields = {logspine::kv("order_id", 42)};

  const auto file_path = temp_dir.path / "events.jsonl";
  {
    logspine::sinks::file_sink sink({.path = file_path, .format = logspine::sink_format::json_lines, .append = false});
    sink.write(event);
    sink.flush();
  }

  const auto file_content = read_all(file_path);
  REQUIRE(file_content.find("\"logger\":\"checkout\"") != std::string::npos);
  REQUIRE_FALSE(file_content.empty());
  REQUIRE(file_content.back() == '\n');

  const auto bulk_path = temp_dir.path / "bulk.ndjson";
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
  const temp_directory temp_dir;
  const auto file_path = temp_dir.path / "custom.log";

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
  const temp_directory temp_dir;
  const auto file_path = temp_dir.path / "rotation.log";

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
  REQUIRE(std::filesystem::exists(temp_dir.path / "rotation.log.1"));
  REQUIRE(std::filesystem::exists(temp_dir.path / "rotation.log.2"));
  REQUIRE_FALSE(std::filesystem::exists(temp_dir.path / "rotation.log.3"));
}

TEST_CASE("file sink preserves the active log when rotation fails", "[sinks][file][rotation]") {
  const temp_directory temp_dir;
  const auto path = temp_dir.path / "events.log";
  const auto archive = temp_dir.path / "events.log.1";
  std::filesystem::create_directories(archive);
  std::ofstream(archive / "entry") << "prevent removal of archive directory";

  logspine::sinks::file_sink sink({.path = path, .append = false, .max_file_size = 1, .max_files = 1});
  logspine::log_event event;
  event.message = "preserve this record";
  sink.write(event);
  sink.flush();
  const auto original = read_all(path);
  REQUIRE_FALSE(original.empty());

  event.message = "next record";
  REQUIRE_THROWS_AS(sink.write(event), std::filesystem::filesystem_error);
  REQUIRE(read_all(path) == original);

  std::filesystem::remove(archive / "entry");
  std::filesystem::remove(archive);
  sink.write(event);
  sink.flush();
  REQUIRE(read_all(archive) == original);
  REQUIRE(read_all(path).find("next record") != std::string::npos);
}

namespace {

struct formatter_lifetime {
  std::latch entered{1};
  std::latch release{1};
  std::atomic<bool> destroyed{false};
};

class blocking_formatter final : public logspine::formatter {
public:
  explicit blocking_formatter(formatter_lifetime& lifetime) : lifetime_(lifetime) {}

  ~blocking_formatter() override {
    lifetime_.destroyed.store(true);
  }

  void format(const logspine::log_event&, std::string& output) override {
    auto& lifetime = lifetime_;
    lifetime.entered.count_down();
    lifetime.release.wait();
    output = "previous formatter\n";
  }

private:
  formatter_lifetime& lifetime_;
};

} // namespace

TEST_CASE("replacing a formatter keeps an active callback alive", "[sinks][extensibility]") {
  const temp_directory temp_dir;
  const auto path = temp_dir.path / "events.log";
  formatter_lifetime lifetime;
  auto sink = std::make_shared<logspine::sinks::file_sink>(logspine::sinks::file_sink_options{.path = path});
  sink->set_formatter(std::make_unique<blocking_formatter>(lifetime));
  logspine::async_dispatcher dispatcher({sink});

  dispatcher.dispatch({});
  lifetime.entered.wait();
  sink->set_formatter(std::make_unique<custom_formatter>());
  const auto destroyed_during_callback = lifetime.destroyed.load();
  lifetime.release.count_down();

  logspine::log_event event;
  event.message = "replacement";
  dispatcher.dispatch(std::move(event));
  dispatcher.flush();

  REQUIRE_FALSE(destroyed_during_callback);
  REQUIRE(lifetime.destroyed.load());
  REQUIRE(read_all(path) == "previous formatter\nCUSTOM_FORMAT: replacement\n");
}
