#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <logspine/logspine.hpp>

namespace {

template <typename Fn>
double benchmark_ns_per_op(std::size_t iterations, Fn&& fn) {
  const auto start = std::chrono::steady_clock::now();
  for (std::size_t index = 0; index < iterations; ++index) {
    fn();
  }
  const auto end = std::chrono::steady_clock::now();
  const auto total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  return static_cast<double>(total_ns) / static_cast<double>(iterations);
}

struct benchmark_options {
  std::size_t iterations = 100000;
  std::size_t threads = 1;
  std::size_t queue_capacity = 4096;
};

benchmark_options parse_options(int argc, char** argv) {
  benchmark_options options;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    auto require_value = [&](std::string_view flag) -> std::string_view {
      if (index + 1 >= argc) {
        throw std::runtime_error("missing value for benchmark option " + std::string(flag));
      }
      ++index;
      return argv[index];
    };

    if (argument == "--iterations") {
      options.iterations = static_cast<std::size_t>(std::stoull(std::string(require_value(argument))));
    } else if (argument == "--threads") {
      options.threads = static_cast<std::size_t>(std::stoull(std::string(require_value(argument))));
    } else if (argument == "--queue-capacity") {
      options.queue_capacity = static_cast<std::size_t>(std::stoull(std::string(require_value(argument))));
    } else {
      options.iterations = static_cast<std::size_t>(std::stoull(std::string(argument)));
    }
  }

  if (options.iterations == 0U) {
    throw std::runtime_error("iterations must be greater than zero");
  }
  if (options.threads == 0U) {
    throw std::runtime_error("threads must be greater than zero");
  }
  if (options.queue_capacity == 0U) {
    throw std::runtime_error("queue-capacity must be greater than zero");
  }

  return options;
}

}  // namespace

int main(int argc, char** argv) {
  const auto options = parse_options(argc, argv);

  auto noop = std::make_shared<logspine::noop_sink>();
  auto sync = std::make_shared<logspine::sync_dispatcher>(std::vector<std::shared_ptr<logspine::sink>>{noop});
  auto async = std::make_shared<logspine::async_dispatcher>(
      std::vector<std::shared_ptr<logspine::sink>>{noop},
      logspine::async_options{
          .queue_capacity = options.queue_capacity,
          .overflow = logspine::overflow_policy::drop_newest,
          .batch_size = 64,
      });
  auto file = std::make_shared<logspine::sinks::file_sink>(logspine::sinks::file_sink_options{
      .path = std::filesystem::temp_directory_path() / "logspine-benchmark.jsonl",
      .format = logspine::sink_format::json_lines,
      .append = false,
  });
  auto file_dispatcher =
      std::make_shared<logspine::sync_dispatcher>(std::vector<std::shared_ptr<logspine::sink>>{file});

  logspine::logger disabled_logger("bench.disabled", sync, logspine::level::info);
  logspine::logger sync_logger("bench.sync", sync, logspine::level::debug);
  logspine::logger async_logger("bench.async", async, logspine::level::debug);
  logspine::logger file_logger("bench.file", file_dispatcher, logspine::level::debug);

  const auto disabled_ns = benchmark_ns_per_op(options.iterations, [&disabled_logger] {
    LOGSPINE_DEBUG(disabled_logger, "suppressed", logspine::kv("value", 42));
  });

  const auto sync_ns = benchmark_ns_per_op(options.iterations, [&sync_logger] {
    sync_logger.info("enabled");
  });

  const auto structured_ns = benchmark_ns_per_op(options.iterations, [&sync_logger] {
    sync_logger.info("structured", {logspine::kv("order_id", 42), logspine::kv("amount", 19.95)});
  });

  const auto file_ns = benchmark_ns_per_op(options.iterations, [&file_logger] {
    file_logger.info("file", {logspine::kv("order_id", 42), logspine::kv("amount", 19.95)});
  });
  file_dispatcher->flush();

  const auto async_start = std::chrono::steady_clock::now();
  std::vector<std::thread> workers;
  workers.reserve(options.threads);
  for (std::size_t thread_index = 0; thread_index < options.threads; ++thread_index) {
    workers.emplace_back([&, thread_index] {
      for (std::size_t index = thread_index; index < options.iterations; index += options.threads) {
        async_logger.info("async", {logspine::kv("iteration", static_cast<std::uint64_t>(index))});
      }
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  async->flush();
  const auto async_end = std::chrono::steady_clock::now();
  const auto async_seconds = std::chrono::duration<double>(async_end - async_start).count();
  const auto async_events_per_sec = static_cast<double>(options.iterations) / async_seconds;

  std::cout << "disabled_debug_ns_per_op=" << disabled_ns << '\n';
  std::cout << "enabled_info_noop_ns_per_op=" << sync_ns << '\n';
  std::cout << "enabled_structured_ns_per_op=" << structured_ns << '\n';
  std::cout << "file_sink_ns_per_op=" << file_ns << '\n';
  std::cout << "async_events_per_sec=" << async_events_per_sec << '\n';
  std::cout << "async_dropped_events=" << async->dropped_events() << '\n';
  std::cout << "threads=" << options.threads << '\n';
  std::cout << "queue_capacity=" << options.queue_capacity << '\n';
  return 0;
}
