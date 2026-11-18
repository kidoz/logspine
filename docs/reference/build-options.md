# Build Options

## Meson options

| Option | Default | Effect |
|---|---|---|
| `build_examples` | `true` | Build the executables in `examples/`. |
| `build_tests` | `true` | Build the Catch2 test executables. |
| `build_benchmarks` | `false` | Build `benchmarks/bench_logging.cpp`. |
| `network` | `true` | Compile network sinks and set `network_enabled` to `true`. |
| `developer_mode` | `false` | Add the strict project warning policy and explicit `-Werror`. |

The project also sets these Meson built-ins by default:

| Built-in | Default |
|---|---|
| `cpp_std` | `c++23,c++latest` |
| `warning_level` | `3` |
| `werror` | `true` |

Developer mode additionally adds `-Werror` to the project warning flags. The
Meson `werror` built-in also defaults to `true`, so warnings fail the build in
every configuration. Pass `-Dwerror=false` to keep warnings non-fatal.

## Dependencies

| Dependency | Required for | Notes |
|---|---|---|
| C++23 compiler | Everything | GCC, Clang, or MSVC. |
| Meson, Ninja, Python 3 | Everything | Build system. |
| Catch2 v3 | `build_tests=true` | Fetched through `subprojects/catch2.wrap`. |
| libcurl | `network=true` | HTTP transport for the OTLP sink. |
| nlohmann/json | `network=true` | OTLP payload and response handling. |
| zlib | Optional | Gzip compression of rotated files and GELF payloads. |

Network builds are enabled by default. Configure with `-Dnetwork=false` when
libcurl or nlohmann/json is unavailable.

## Generated configuration

Meson generates `logspine/config.hpp` from `config.hpp.in`. The header defines
`LOGSPINE_WITH_NETWORK` and the `logspine::network_enabled` constant. When zlib
is found, the build defines `LOGSPINE_WITH_ZLIB`.

## Build targets

| Target | Produced when |
|---|---|
| `logspine` library | Always |
| `logspine_basic_example` | `build_examples=true` |
| `logspine_structured_example` | `build_examples=true` |
| `logspine_elastic_bulk_example` | `build_examples=true` |
| `logspine_graylog_gelf_udp_example` | `build_examples=true` and `network=true` |
| Test executables | `build_tests=true` |
| `bench_logging` | `build_benchmarks=true` |
| `format`, `format-check` | `clang-format` is installed |
| `clang-tidy` | `clang-tidy` is installed |

Test executables are listed in [Test suite](test-suite.md).

## Install layout

`meson install` installs:

- The library under the configured `libdir`.
- Public headers under `includedir/logspine/`.
- The generated `logspine/config.hpp`.

Use `--destdir <path>` for a staged install.
