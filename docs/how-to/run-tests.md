# Run the Test Suite

## Prerequisites

Tests use Catch2 v3 through the Meson wrap `subprojects/catch2.wrap`. The first
`meson setup` downloads and extracts it.

## Run the default suite

```bash
meson setup build -Dbuild_tests=true
meson compile -C build
meson test -C build --print-errorlogs
```

Tests are enabled by default, so `-Dbuild_tests=true` is optional.

## Run selected suites

Pass one or more test names to `meson test`:

```bash
meson test -C build test_json
meson test -C build test_core test_sinks
```

See [Test suite](../reference/test-suite.md) for the executable names and the
coverage of each one.

## Run without network sinks

```bash
meson setup build-nonet -Dnetwork=false -Dbuild_tests=true
meson compile -C build-nonet
meson test -C build-nonet --print-errorlogs
```

## Run with sanitizers

With GCC or Clang:

```bash
meson setup build-asan -Db_sanitize=address,undefined -Dbuild_tests=true -Ddeveloper_mode=true
meson compile -C build-asan
meson test -C build-asan --print-errorlogs
```

## Add a test

1. Add a `tests/test_<area>.cpp` file using Catch2 v3.
2. Register the executable in the `tests` dictionary in `meson.build`.
3. Put network-dependent tests in the `if network_enabled` branch, or add a
   separate case that runs when networking is disabled.
4. Keep the default suite free of external services. Loopback-only transport
   tests are acceptable behind `-Dnetwork=true`.

Prefer deterministic payload assertions over timing-sensitive integration
behavior.
