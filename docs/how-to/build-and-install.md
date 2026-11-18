# Build and Install LogSpine

## Prerequisites

- A compiler with C++23 support.
- Meson, Ninja, and Python 3.
- Catch2 v3 for tests, downloaded through the Meson wrap.
- libcurl and nlohmann/json development files for network sinks.
- zlib for optional compression of rotated files and GELF payloads.

Network sinks are enabled by default. If libcurl or nlohmann/json is not
available, configure with `-Dnetwork=false`.

## Configure and build

```bash
meson setup build
meson compile -C build
```

The defaults enable examples and tests, disable benchmarks, and enable network
sinks. See [Build options](../reference/build-options.md) for every option and
its default.

## Build without network sinks

```bash
meson setup build-nonet -Dnetwork=false
meson compile -C build-nonet
meson test -C build-nonet --print-errorlogs
```

The library, local sinks, examples, and default tests still build and pass.

## Build only the library

```bash
meson setup build-lib -Dbuild_examples=false -Dbuild_tests=false
meson compile -C build-lib
```

## Build with benchmarks

```bash
meson setup build-bench -Dbuild_benchmarks=true
meson compile -C build-bench
```

Run the benchmark as described in [Run benchmarks](run-benchmarks.md).

## Enable strict warnings

```bash
meson setup build-dev -Dbuild_tests=true -Ddeveloper_mode=true
meson compile -C build-dev
```

Developer mode adds the project's strict `-Werror` flag from `meson.build`.
Warnings are already errors by default because the project sets
`werror=true`; pass `-Dwerror=false` to keep them non-fatal.

## Install

```bash
meson install -C build
```

To install into a staging directory without writing to the system prefix:

```bash
meson install -C build --destdir install-root
```

Installation places the library, the public headers, and the generated
`logspine/config.hpp` under the configured prefix.
