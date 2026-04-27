# Integrations

## Console and File

`console_sink` supports human-readable and JSON Lines output. Warnings and above default to `stderr`. `file_sink` writes either compact JSON Lines or the same human-readable formatter to a file path owned by an RAII `std::ofstream`.

Example local usage:

```cpp
auto console = std::make_shared<logspine::sinks::console_sink>(
    logspine::sinks::console_sink_options{.format = logspine::sink_format::human});
auto file = std::make_shared<logspine::sinks::file_sink>(
    logspine::sinks::file_sink_options{.path = "application.jsonl"});
```

## Elasticsearch Bulk NDJSON

`elastic_bulk_file_sink` writes offline bulk payloads shaped as:

```text
{"index":{"_index":"logs-index"}}
{"@timestamp":"...","level":"info","logger":"...","message":"...","fields":{"order_id":42}}
```

Each event produces one action line plus one source line, both newline terminated. The resulting file is suitable for later submission to Elasticsearch Bulk API tooling or an intermediary collector.

Example upload flow:

```bash
curl -H 'Content-Type: application/x-ndjson' \
  -XPOST 'http://localhost:9200/_bulk' \
  --data-binary @logspine-bulk.ndjson
```

## TCP JSON Lines

`tcp_json_lines_sink` sends one compact JSON record per line over TCP. The current implementation keeps configuration strongly typed and performs lazy connect on first write.

- `lazy_connect` controls whether the socket is opened in the constructor or on first write.
- `reconnect_on_failure` enables reconnect-based retries after transport failures.
- `max_write_retries` controls how many reconnect/retry attempts are made for a single write.
- sink instances own their transport state directly; there is no hidden global sink transport registry.
- `statistics()` and `last_error_message()` expose sink-local transport observability.

Still needed for production-grade use:

- reconnect backoff and jitter
- connect/send timeout tuning
- TLS and authentication
- metrics/export for sink health beyond the local failure counter

Example Logstash pipeline:

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

## Graylog GELF UDP

`gelf_udp_sink` maps LogSpine levels to GELF/syslog severities and writes JSON payloads over UDP. Custom structured fields are emitted with the GELF `_` prefix.

- `reconnect_on_failure` re-resolves the destination and retries after send failures.
- `max_write_retries` controls how many reconfiguration retries are made for a single write.
- payload generation is deterministic and covered by local tests without requiring a Graylog instance.
- `statistics()` and `last_error_message()` expose sink-local transport observability.

Still needed for production-grade use:

- UDP size and chunking controls
- optional compression
- alternative TCP or HTTP GELF transports

Example Graylog input configuration:

```text
System -> Inputs -> Select GELF UDP -> Launch new input
Bind address: 0.0.0.0
Port: 12201
```

## Future OTLP

OpenTelemetry OTLP remains future work. The current event model already keeps room for trace, span, and correlation identifiers so an OTLP sink can translate events into log records without changing the logger API.
