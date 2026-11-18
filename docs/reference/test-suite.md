# Test Suite

Tests use Catch2 v3 from the Meson wrap. Each executable is registered in
`meson.build`.

## Executables

| Executable | Built when | Coverage |
|---|---|---|
| `test_core` | `build_tests=true` | Level helpers, registry identity, disabled macro paths, level changes, and flush behavior. |
| `test_json` | `build_tests=true` | JSON escaping, event serialization, and human-readable records. |
| `test_async_dispatcher` | `build_tests=true` | Flush ordering, sink failure counting, overflow policies, shutdown draining, and concurrent flush. |
| `test_sinks` | `build_tests=true` | File and Elasticsearch record shapes, payload framing, custom formatters and filters, rotation, and formatter lifetime. |
| `test_mdc` | `build_tests=true` | Thread-local diagnostic context. |
| `test_format` | `build_tests=true` | `std::format` message macros, plain strings, and field arguments. |
| `test_network_sinks` | `build_tests=true` and `network=true` | Loopback TCP and UDP delivery, GELF chunk validation, and OTLP acknowledgement and attribute types. |
| `test_otlp` | `build_tests=true` and `network=false` | OTLP behavior when networking is disabled. |

## External service policy

The default suite does not require Elasticsearch, Logstash, Graylog, or an
OpenTelemetry collector. Network tests use loopback sockets only. Richer
integration tests can be added behind separate opt-in Meson options.

## Listing and selecting tests

```bash
meson test -C build --list
meson test -C build test_json
```

See [Run tests](../how-to/run-tests.md) for the full command set.
