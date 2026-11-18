# LogSpine

[![CI](https://github.com/kidoz/logspine/actions/workflows/ci.yml/badge.svg)](https://github.com/kidoz/logspine/actions/workflows/ci.yml)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Build: Meson](https://img.shields.io/badge/build-Meson-264478.svg)](https://mesonbuild.com/)
[![Platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)](#build)
[![Version](https://img.shields.io/badge/version-0.1.0-informational.svg)](meson.build)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

LogSpine is a C++23 structured logging library with a small facade, pluggable
sinks, bounded async delivery, and a Meson build. It is designed for Linux,
Windows, and macOS, with local sinks implemented first and network sinks exposed
behind the same public API.

## Documentation

**Tutorials**

- [Getting started](docs/tutorials/getting-started.md) — build the library and
  emit your first log records.

**How-to guides**

- [Build and install](docs/how-to/build-and-install.md)
- [Run tests](docs/how-to/run-tests.md)
- [Run formatting and static analysis](docs/how-to/run-tooling.md)
- [Run benchmarks](docs/how-to/run-benchmarks.md)
- [Configure sinks](docs/how-to/configure-sinks.md)

**Reference**

- [Public API](docs/reference/public-api.md)
- [Sinks and options](docs/reference/sinks.md)
- [Build options](docs/reference/build-options.md)
- [Test suite](docs/reference/test-suite.md)

**Explanation**

- [Architecture](docs/explanation/architecture.md)
- [Performance](docs/explanation/performance.md)
- [Research notes](docs/explanation/research.md)

## Features

- Small `logger` and `logger_registry` facade inspired by SLF4J.
- Structured fields via `kv()` helpers.
- Deterministic JSON serialization with safe escaping.
- `sync_dispatcher` and bounded `async_dispatcher`.
- Console, file, and Elasticsearch bulk NDJSON sinks.
- Optional TCP JSON Lines, Graylog GELF UDP, and OTLP/HTTP JSON sinks through the
  same sink interface.
- Catch2 v3 test suite through a Meson wrap.

## Quick start

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

For a complete walkthrough, see [Getting started](docs/tutorials/getting-started.md).

## Build

Network builds require the libcurl development files and nlohmann/json headers.
Tests use Catch2 v3 through the Meson wrap at `subprojects/catch2.wrap`.

```bash
meson setup build -Dbuild_examples=true -Dbuild_tests=true -Ddeveloper_mode=true
meson compile -C build
meson test -C build --print-errorlogs
```

To build without network sinks:

```bash
meson setup build-nonet -Dnetwork=false -Dbuild_tests=true
meson compile -C build-nonet
meson test -C build-nonet --print-errorlogs
```

See [Build and install](docs/how-to/build-and-install.md) for the full option
set and sanitizer, tooling, and install commands.

## Examples

- `examples/basic.cpp`: minimal console logging with `sync_dispatcher`
- `examples/structured.cpp`: structured JSON-lines logging with `async_dispatcher`
- `examples/elastic_bulk_file.cpp`: writes offline Elasticsearch bulk NDJSON
- `examples/graylog_gelf_udp.cpp`: demonstrates GELF UDP configuration for a
  Graylog listener
