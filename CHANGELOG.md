# Changelog

All notable changes to LogSpine are documented in this file.

## [Unreleased]

### Added

- **Core API:** `logger` and `logger_registry` with stable named loggers, atomic
  level gating, runtime level changes, and disabled-level short-circuiting.
- **Structured fields:** typed `field` values (`nullptr`, `bool`, signed and
  unsigned integers, `double`, `std::string`) created through `kv()` and copied
  into owned event storage.
- **Events:** `log_event` with owned message and field storage, timestamp, logger
  name, thread id, optional source location, and optional trace, span, and
  correlation identifiers.
- **JSON:** deterministic serialization with escaping for messages, field keys,
  and field values.
- **Sink pipeline:** `sink` interface with pluggable `formatter` and `filter`
  hooks, plus `noop_sink`.
- **Dispatchers:** `sync_dispatcher` for inline delivery and bounded
  `async_dispatcher` with a dedicated worker thread, batching, ordered flush,
  and graceful shutdown.
- **Overflow handling:** `block`, `drop_newest`, and `drop_oldest` policies with
  dropped-event accounting; sink exceptions are isolated and counted.
- **Queue:** bounded lock-free MPSC queue backing the async dispatcher.
- **Macros:** variadic `LOGSPINE_TRACE`, `LOGSPINE_DEBUG`, `LOGSPINE_INFO`,
  `LOGSPINE_WARN`, `LOGSPINE_ERROR`, and `LOGSPINE_FATAL` macros with
  `std::format` message support.
- **Context:** thread-local mapped diagnostic context through `mdc` and
  `scoped_mdc`.
- **Console sink:** human-readable and JSON Lines output, with warnings and
  above sent to `stderr` by default.
- **File sink:** JSON Lines or human-readable output, append mode, size-based
  rotation, rotation-file retention via `max_files`, and optional gzip
  compression of rotated files when zlib is available.
- **Elasticsearch sink:** `elastic_bulk_file_sink` producing bulk NDJSON with
  one action line and one source line per event.
- **TCP sink:** `tcp_json_lines_sink` with lazy connection, reconnect handling,
  exponential retry backoff, socket timeouts, and sink-local statistics.
- **GELF sink:** `gelf_udp_sink` with GELF severity mapping, `_`-prefixed
  custom fields, optional zlib compression, and UDP chunking up to the GELF
  128-chunk limit.
- **OTLP sink:** `otlp_http_sink` sending OTLP/HTTP JSON log records to
  `/v1/logs` with a configurable `service.name`, retries, and sink-local
  statistics.
- **Build:** Meson project targeting C++23, with `network`, `build_examples`,
  `build_tests`, `build_benchmarks`, and `developer_mode` options; optional zlib
  support; and install rules for the library and public headers.
- **Tests:** Catch2 v3 suites covering core logging, JSON, async dispatching,
  sinks, MDC, formatting, and OTLP, plus loopback TCP/UDP tests when network
  sinks are enabled.
- **Examples and benchmarks:** basic console logging, structured async logging,
  Elasticsearch bulk output, Graylog GELF UDP, and a logging micro-benchmark.
- **Tooling:** `format`, `format-check`, and `clang-tidy` Meson targets.
- **CI:** GCC and Clang on Linux, Clang on macOS, and MSVC on Windows, plus a
  no-network build, formatting and static-analysis gates, and ASan/UBSan runs.
- **Documentation:** architecture, integrations, performance, research, testing,
  and tooling guides.

[Unreleased]: https://github.com/kidoz/logspine/commits/main
