# LogSpine

LogSpine is a C++23 structured logging library with a small facade, pluggable sinks, bounded async delivery, and a Meson build. It is designed for Linux, Windows, and macOS, with local sinks implemented first and network sinks exposed behind the same public API.

## Features

- Small `logger` and `logger_registry` facade inspired by SLF4J.
- Structured fields via `kv()` helpers.
- Deterministic JSON serialization with safe escaping.
- `sync_dispatcher` and bounded `async_dispatcher`.
- Console, file, and Elasticsearch bulk NDJSON sinks.
- Optional TCP JSON Lines and Graylog GELF UDP sinks through the same sink interface.
- Catch2 v3 test suite through a Meson wrap.

## Build

Tests use Catch2 v3 via the Meson wrap at `subprojects/catch2.wrap`.

Meson options used by the project:

- `-Dbuild_examples=true|false`
- `-Dbuild_tests=true|false`
- `-Dbuild_benchmarks=true|false`
- `-Dnetwork=true|false`
- `-Ddeveloper_mode=true|false`

```bash
meson setup build -Dbuild_examples=true -Dbuild_tests=true -Ddeveloper_mode=true
meson compile -C build
meson test -C build --print-errorlogs
```

Disable network sinks:

```bash
meson setup build-nonet -Dnetwork=false -Dbuild_tests=true
meson compile -C build-nonet
meson test -C build-nonet --print-errorlogs
```

Sanitizer-friendly setup on GCC or Clang:

```bash
meson setup build-asan -Db_sanitize=address,undefined -Dbuild_tests=true -Ddeveloper_mode=true
meson compile -C build-asan
meson test -C build-asan --print-errorlogs
```

Tooling targets when the tools are installed:

```bash
meson compile -C build format-check
meson compile -C build clang-tidy
```

Install smoke test:

```bash
meson install -C build --destdir install-root
```

## Quick Start

```cpp
#include <memory>
#include <vector>

#include <logspine/logspine.hpp>

int main() {
  auto console = std::make_shared<logspine::sinks::console_sink>(
      logspine::sinks::console_sink_options{.format = logspine::sink_format::human});
  auto dispatcher = std::make_shared<logspine::async_dispatcher>(
      std::vector<std::shared_ptr<logspine::sink>>{console},
      logspine::async_options{.queue_capacity = 8192, .overflow = logspine::overflow_policy::drop_oldest, .batch_size = 64});

  logspine::logger_registry registry(dispatcher, logspine::level::debug);
  auto log = registry.get("checkout.service");

  log->info("order accepted", {
      logspine::kv("order_id", 42),
      logspine::kv("customer", "alice"),
      logspine::kv("amount", 199.95),
  });

  LOGSPINE_DEBUG(*log, "cache hit", logspine::kv("key", "user:42"));
  registry.flush();
  return 0;
}
```

## Examples

- `examples/basic.cpp`: minimal console logging with `sync_dispatcher`
- `examples/structured.cpp`: structured JSON-lines logging with `async_dispatcher`
- `examples/elastic_bulk_file.cpp`: writes offline Elasticsearch bulk NDJSON
- `examples/graylog_gelf_udp.cpp`: demonstrates GELF UDP configuration for a Graylog listener

## Integration Overview

LogSpine currently ships local sinks for console and files, an offline Elasticsearch bulk NDJSON sink, a TCP JSON-lines sink suitable for Logstash-style collectors, and a GELF UDP sink for Graylog.

## Network Sink Observability

The network sinks expose retry configuration and sink-local stats:

- `tcp_json_lines_sink_options::max_write_retries`
- `gelf_udp_sink_options::max_write_retries`
- `statistics()` for write attempts, write failures, and reconnect attempts
- `last_error_message()` for the most recent transport error

## Architecture Summary

Application code talks to `logger` instances produced by `logger_registry`. Loggers gate disabled levels before building `log_event` payloads, then forward accepted events to a `dispatcher`. Dispatchers serialize access to one or more `sink` implementations, either inline (`sync_dispatcher`) or through a bounded queue (`async_dispatcher`).

See [docs/architecture.md](docs/architecture.md), [docs/integrations.md](docs/integrations.md), [docs/performance.md](docs/performance.md), [docs/testing.md](docs/testing.md), and [docs/research.md](docs/research.md).
