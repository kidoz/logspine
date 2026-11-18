# Sinks and Options

A sink writes `log_event` values. Every sink implements `write()` and `flush()`
and accepts an optional `formatter` and `filter` through `set_formatter()` and
`set_filter()`.

## `sink_format`

```cpp
enum class sink_format {
  human,
  json_lines,
};
```

## Console sink

```cpp
struct console_sink_options {
  sink_format format = sink_format::human;
  bool use_stderr_for_warnings = true;
};
```

`console_sink` writes to `std::cout`. With `use_stderr_for_warnings`, warnings
and above are written to `std::cerr`.

## File sink

```cpp
struct file_sink_options {
  std::filesystem::path path;
  sink_format format = sink_format::json_lines;
  bool append = true;
  std::size_t max_file_size = 0;
  std::uint32_t max_files = 0;
  bool compress_rotated = false;
};
```

| Option | Meaning |
|---|---|
| `path` | Output file path. Required. |
| `format` | Human-readable or JSON Lines output. |
| `append` | Append to an existing file instead of truncating it. |
| `max_file_size` | Rotate at this size in bytes. `0` disables rotation. |
| `max_files` | Number of rotated files to retain. `0` keeps all. |
| `compress_rotated` | Write gzip archives when zlib support is compiled in. |

## Elasticsearch bulk file sink

```cpp
struct elastic_bulk_file_sink_options {
  std::filesystem::path path;
  std::string index_name = "logspine";
  bool append = true;
};
```

Each event writes an action line followed by a source line. By default, the
source line is the same JSON record produced by `to_json_lines_record()`. A
custom formatter replaces that record.

## TCP JSON Lines sink

```cpp
struct tcp_json_lines_sink_options {
  std::string host = "127.0.0.1";
  std::uint16_t port = 5000;
  bool lazy_connect = true;
  bool reconnect_on_failure = true;
  std::uint32_t max_write_retries = 1;
};
```

| Option | Meaning |
|---|---|
| `host`, `port` | Destination endpoint. |
| `lazy_connect` | Connect on first write instead of in the constructor. |
| `reconnect_on_failure` | Reconnect and retry after transport failures. |
| `max_write_retries` | Retry attempts for one write. |

The sink writes one compact JSON object plus `\n` per event. It exposes
`connected()`, `write_failures()`, `statistics()`, and `last_error_message()`.

## Graylog GELF UDP sink

```cpp
struct gelf_udp_sink_options {
  std::string host = "127.0.0.1";
  std::uint16_t port = 12201;
  std::string source_host = "localhost";
  bool reconnect_on_failure = true;
  std::uint32_t max_write_retries = 1;
  bool compress = false;
  std::size_t max_chunk_size = 1024;
};
```

| Option | Meaning |
|---|---|
| `host`, `port` | Graylog GELF UDP input endpoint. |
| `source_host` | Value of the GELF `host` field. |
| `reconnect_on_failure` | Re-resolve the destination and retry after failures. |
| `max_write_retries` | Retry attempts for one write. |
| `compress` | Compress payloads with zlib when available. |
| `max_chunk_size` | Maximum UDP chunk size in bytes (13 to 65507). |

The payload contains `version`, `host`, `short_message`, `timestamp`, `level`,
`_logger`, and `_`-prefixed custom fields. Records requiring more than 128
chunks are reported as failed.

The sink exposes `write_failures()`, `statistics()`, and
`last_error_message()`.

## OTLP/HTTP sink

```cpp
struct otlp_http_sink_options {
  std::string host = "127.0.0.1";
  std::uint16_t port = 4318;
  std::string path = "/v1/logs";
  std::string service_name = "unknown_service";
  bool lazy_connect = true;
  bool reconnect_on_failure = true;
  std::uint32_t max_write_retries = 1;
};
```

| Option | Meaning |
|---|---|
| `host`, `port` | Collector endpoint. |
| `path` | HTTP path for the logs endpoint. |
| `service_name` | Value of the OTLP resource attribute `service.name`. |
| `lazy_connect` | Verify TCP connectivity in the constructor when `false`. |
| `reconnect_on_failure` | Retry transient connection failures when `true`. |
| `max_write_retries` | Retry attempts for one write. |

The sink posts one OTLP JSON log record per request, checks the collector
acknowledgement, and reports rejected records. It exposes `connected()`,
`write_failures()`, `statistics()`, and `last_error_message()`.

## Network sink statistics

```cpp
struct network_sink_statistics {
  std::uint64_t write_attempts = 0;
  std::uint64_t write_failures = 0;
  std::uint64_t reconnect_attempts = 0;
};
```

TCP, GELF, and OTLP sinks return these counters from `statistics()`.

## Output formats

### Human-readable

```text
<timestamp> <level> <logger> <message>[ <key>=<value>]...
```

The timestamp is UTC in ISO-8601 form with millisecond precision. User text and
field values have control characters escaped.

### JSON Lines

```json
{"@timestamp":"...","level":"info","logger":"...","message":"...","thread_id":"...","fields":{"key":"value"}}
```

Optional keys appear before `fields`: `file`, `line`, `function`, `trace_id`,
`span_id`, and `correlation_id`.

### GELF

```json
{"version":"1.1","host":"...","short_message":"...","timestamp":0.0,"level":6,"_logger":"...","_key":"value"}
```

### OTLP/HTTP

The request body has the shape `resourceLogs[].scopeLogs[].logRecords[]` with a
`service.name` resource attribute. Log records carry `timeUnixNano`,
`severityText`, `severityNumber`, `body`, `attributes`, and optional `traceId`
and `spanId`.
