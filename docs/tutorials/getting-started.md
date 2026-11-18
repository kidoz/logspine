# Getting Started

This tutorial builds LogSpine, runs the bundled examples, and confirms that
structured logging works. It takes about ten minutes.

## Before you begin

You need:

- A compiler with C++23 support (GCC, Clang, or MSVC).
- Meson and Ninja.
- Python 3 for Meson.
- Git.

Network sinks also need the libcurl development files and nlohmann/json
headers. If those are not installed, use `-Dnetwork=false` in the configure
step.

## 1. Get the source

```bash
git clone https://github.com/kidoz/logspine.git
cd logspine
```

## 2. Configure the build

```bash
meson setup build
```

The defaults build the library, examples, and tests, with network sinks
enabled. If the network dependencies are missing, configure without them:

```bash
meson setup build -Dnetwork=false
```

## 3. Build

```bash
meson compile -C build
```

## 4. Run the first example

```bash
./build/logspine_basic_example
```

The example uses `sync_dispatcher` and `console_sink` with human-readable
output. You should see one line similar to:

```text
2026-11-18T12:34:56.789Z info example.basic service started
```

The timestamp is UTC and includes milliseconds. The level is lowercase.

## 5. Run the structured example

```bash
./build/logspine_structured_example
```

This example uses `async_dispatcher` and JSON Lines output. Each event is one
JSON object on its own line. A representative record is:

```json
{"@timestamp":"2026-11-18T12:34:56.789Z","level":"info","logger":"checkout.service","message":"order accepted","thread_id":"0x1","fields":{"order_id":42,"customer":"alice","amount":199.95}}
```

Real records also include source-location keys (`file`, `line`, `function`) and
may include trace, span, and correlation identifiers. The thread id and
timestamp vary between runs. The second event is the debug record emitted by
the `LOGSPINE_DEBUG` macro.

## 6. Run the tests

```bash
meson test -C build --print-errorlogs
```

All tests should pass. Network tests use loopback sockets and do not require
Elasticsearch, Logstash, Graylog, or an OpenTelemetry collector.

## Next steps

- [Configure sinks](../how-to/configure-sinks.md) for files, Elasticsearch,
  TCP, Graylog, and OTLP/HTTP.
- [Public API](../reference/public-api.md) for the full type and macro surface.
- [Architecture](../explanation/architecture.md) for the design behind the
  facade, dispatchers, and sinks.
