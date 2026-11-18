# Configure Sinks

A sink receives `log_event` values from a dispatcher. Construct one or more
sinks, place them in a vector, and pass the vector to `sync_dispatcher` or
`async_dispatcher`:

```cpp
#include <memory>
#include <vector>

#include <logspine/logspine.hpp>

auto console = std::make_shared<logspine::sinks::console_sink>();
auto dispatcher = std::make_shared<logspine::sync_dispatcher>(
    std::vector<std::shared_ptr<logspine::sink>>{console});
```

The examples below focus on the task; see [Sinks and options](../reference/sinks.md)
for every option and default.

## Write to the console

```cpp
auto console = std::make_shared<logspine::sinks::console_sink>(
    logspine::sinks::console_sink_options{
        .format = logspine::sink_format::human,
        .use_stderr_for_warnings = true,
    });
```

- `sink_format::human` writes `timestamp level logger message key=value...`.
- `sink_format::json_lines` writes one JSON object per line.
- With `use_stderr_for_warnings = true`, warnings and above go to `stderr`.

## Write to a file with rotation

```cpp
auto file = std::make_shared<logspine::sinks::file_sink>(
    logspine::sinks::file_sink_options{
        .path = "logs/application.jsonl",
        .format = logspine::sink_format::json_lines,
        .append = true,
        .max_file_size = 10 * 1024 * 1024,
        .max_files = 5,
        .compress_rotated = true,
    });
```

- `max_file_size = 0` disables rotation.
- `max_files` controls how many rotated files are retained.
- `compress_rotated` writes gzip archives when LogSpine is built with zlib;
  otherwise the option is ignored.

## Write Elasticsearch bulk NDJSON

```cpp
auto bulk = std::make_shared<logspine::sinks::elastic_bulk_file_sink>(
    logspine::sinks::elastic_bulk_file_sink_options{
        .path = "logspine-bulk.ndjson",
        .index_name = "checkout-events",
        .append = false,
    });
```

Each event produces an action line and a source line:

```text
{"index":{"_index":"checkout-events"}}
{"@timestamp":"...","level":"info","logger":"...","message":"...","thread_id":"...","fields":{"order_id":42}}
```

The file is offline NDJSON. Submit it with the Elasticsearch Bulk API or an
intermediary collector:

```bash
curl -H 'Content-Type: application/x-ndjson' \
  -XPOST 'http://localhost:9200/_bulk' \
  --data-binary @logspine-bulk.ndjson
```

## Send JSON Lines over TCP

```cpp
auto tcp = std::make_shared<logspine::sinks::tcp_json_lines_sink>(
    logspine::sinks::tcp_json_lines_sink_options{
        .host = "127.0.0.1",
        .port = 5000,
        .lazy_connect = true,
        .reconnect_on_failure = true,
        .max_write_retries = 3,
    });
```

The sink writes one compact JSON record plus `\n` per event. It reconnects and
retries with exponential backoff when configured. A Logstash receiver can use
the default line framing:

```text
input {
  tcp {
    port => 5000
    codec => json_lines
  }
}

output {
  stdout { codec => rubydebug }
}
```

The transport is plaintext. Use a trusted network or a local collector.

## Send GELF to Graylog over UDP

```cpp
auto gelf = std::make_shared<logspine::sinks::gelf_udp_sink>(
    logspine::sinks::gelf_udp_sink_options{
        .host = "127.0.0.1",
        .port = 12201,
        .source_host = "checkout-service",
        .reconnect_on_failure = true,
        .max_write_retries = 1,
        .compress = true,
        .max_chunk_size = 1024,
    });
```

Custom fields are emitted with the GELF `_` prefix. `compress` uses zlib when
available. Payloads larger than `max_chunk_size` are chunked; records that would
require more than 128 chunks are reported as failed instead of being sent.

Configure a GELF UDP input in Graylog:

```text
System -> Inputs -> Select GELF UDP -> Launch new input
Bind address: 0.0.0.0
Port: 12201
```

## Send OTLP/HTTP logs

```cpp
auto otlp = std::make_shared<logspine::sinks::otlp_http_sink>(
    logspine::sinks::otlp_http_sink_options{
        .host = "127.0.0.1",
        .port = 4318,
        .path = "/v1/logs",
        .service_name = "checkout-service",
        .lazy_connect = true,
        .reconnect_on_failure = true,
        .max_write_retries = 1,
    });
```

The sink posts OTLP JSON and checks the collector acknowledgement. Rejected
records and collector warnings are surfaced through `last_error_message()`.
Each attempt has a five-second timeout and reads at most 4 MiB of response
data. OTLP/HTTP builds require libcurl and nlohmann/json.

## Inspect network sink health

The TCP, GELF, and OTLP sinks expose sink-local counters and the last transport
error:

```cpp
const auto stats = tcp->statistics();
// stats.write_attempts, stats.write_failures, stats.reconnect_attempts

const auto message = tcp->last_error_message();
const bool connected = tcp->connected(); // TCP and OTLP
```

## Add a formatter or filter

Every sink accepts a formatter and a filter:

```cpp
class level_filter final : public logspine::filter {
public:
  explicit level_filter(logspine::level minimum) : minimum_(minimum) {}

  bool accept(const logspine::log_event& event) override {
    return event.severity >= minimum_;
  }

private:
  logspine::level minimum_;
};

console->set_filter(std::make_unique<level_filter>(logspine::level::warn));
```

A custom formatter derives from `logspine::formatter` and writes into the
provided `std::string`:

```cpp
class message_only_formatter final : public logspine::formatter {
public:
  void format(const logspine::log_event& event, std::string& output) override {
    output += logspine::to_string(event.severity);
    output += ' ';
    output += event.message;
    output += '\n';
  }
};

console->set_formatter(std::make_unique<message_only_formatter>());
```

Built-in sinks snapshot the callbacks per invocation, so replacing a formatter
or filter does not affect an invocation that is already running.
