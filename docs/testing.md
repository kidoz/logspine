# Testing

## Default Test Run

The test suite uses Catch2 v3, provided through the Meson wrap file `subprojects/catch2.wrap`.

```bash
meson setup build -Dbuild_tests=true
meson compile -C build
meson test -C build --print-errorlogs
```

Each public header under `include/logspine/` was also compile-checked as a standalone translation unit during the integration pass to confirm self-containment.

## Coverage in This Pass

- `test_core`: levels, registry identity, disabled-level gating, macro short-circuiting.
- `test_json`: escaping, event serialization, human formatter behavior.
- `test_async_dispatcher`: flush, sink exception handling, blocking behavior, drop accounting.
- `test_sinks`: file sink, Elasticsearch bulk NDJSON shape, TCP JSON-lines framing, and GELF payload shape.
- `test_network_sinks`: loopback TCP and UDP receiver tests for the real network sinks, plus sink-local stats checks. This test is built only when `-Dnetwork=true`.

## Sanitizers

```bash
meson setup build-asan -Db_sanitize=address,undefined -Dbuild_tests=true -Ddeveloper_mode=true
meson compile -C build-asan
meson test -C build-asan --print-errorlogs
```

## Adding Tests

- Add normal library behavior tests under `tests/test_*.cpp` and register them through `meson.build`.
- Keep external-service-free tests in the default suite.
- Gate loopback or transport-specific coverage behind `-Dnetwork=true` when it depends on optional network sinks.
- Prefer deterministic payload assertions over timing-sensitive integration behavior.

## External Service Policy

Default tests do not require Elasticsearch, Logstash, Graylog, or an OpenTelemetry collector. With `-Dnetwork=true`, the suite adds loopback-only transport tests for TCP JSON-lines and GELF UDP without requiring any external service. Richer integration tests can still be added later behind separate opt-in Meson options.
