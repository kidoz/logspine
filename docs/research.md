# Research Notes

These references inform the current build/tooling and integration design.

## Inspiration Sources

- SLF4J positions itself as a simple facade that lets users choose the logging backend at deployment time.
  https://www.slf4j.org/manual.html
- Logback documents a split between `Logger`, `Appender`, and `Layout`, plus hierarchical logger naming and effective-level inheritance.
  https://logback.qos.ch/manual/architecture.html
- Serilog emphasizes structured event properties as first-class data rather than plain formatted strings.
  https://serilog.net/

Implication: LogSpine keeps a small facade API, a sink/appender-style backend boundary, and typed structured fields as first-class event data.

## Meson and Clang Tooling

- Meson built-in options: `warning_level`, `werror`, `b_sanitize`, and `cpp_std` are documented in the Meson built-in options reference.
  https://mesonbuild.com/Builtin-options.html
- Meson project build options are documented here.
  https://mesonbuild.com/Build-options.html
- `clang-format` supports project-local `.clang-format` files and LLVM style as a base.
  https://clang.llvm.org/docs/ClangFormat.html
- Detailed style keys live in Clang Format Style Options.
  https://clang.llvm.org/docs/ClangFormatStyleOptions.html
- `clang-tidy` check selection and invocation are documented by LLVM.
  https://clang.llvm.org/extra/clang-tidy/

Implication: the repo uses Meson options for examples/tests/benchmarks/network/developer mode, LLVM-derived formatting, and optional run targets for format and clang-tidy instead of hard-failing setup when tools are missing.

## Elasticsearch Bulk

- Elasticsearch Bulk API requires newline-delimited JSON with an action line and, for `index`, a source line after it.
  https://www.elastic.co/guide/en/elasticsearch/reference/8.19/docs-bulk.html

Implication: the first sink implementation writes offline NDJSON files with exact action/source framing and trailing newlines.

## Graylog GELF

- Graylog documents GELF fields, transports, and support for compression and chunking.
  https://go2docs.graylog.org/current/getting_in_log_data/gelf.html

Implication: the initial GELF sink emits the required JSON fields and `_`-prefixed custom fields, while chunking and compression remain explicit future hardening work.

## Logstash TCP and JSON Lines

- The Logstash TCP input plugin treats each event as one line of text and supports a configurable codec.
  https://www.elastic.co/docs/reference/logstash/plugins/plugins-inputs-tcp
- The `json_lines` codec uses newline-delimited framing by default and parses each delimited JSON line into an event.
  https://www.elastic.co/docs/reference/logstash/plugins/plugins-codecs-json_lines

Implication: the TCP sink emits one JSON object plus `\n` per event, matching the default framing expectation for Logstash-style line-oriented TCP ingestion.

## OpenTelemetry OTLP

- OTLP is stable for logs and documents HTTP/gRPC transport, partial success, and retryable failures.
  https://opentelemetry.io/docs/specs/otlp/
- OpenTelemetry logging explains the log data model and trace/log correlation.
  https://opentelemetry.io/docs/reference/specification/logs/

Implication: `log_event` keeps optional trace/span/correlation identifiers so an OTLP sink can be added without changing the facade.

## Cross-Platform Sockets

- Winsock initialization and version negotiation start with `WSAStartup`.
  https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-wsastartup
- Windows socket shutdown semantics are documented for `closesocket`.
  https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-closesocket
- POSIX/XNS `getaddrinfo()` remains the portable address-resolution API baseline.
  https://pubs.opengroup.org/onlinepubs/009619199/getad.htm

Implication: socket setup is isolated under `src/net/`, public headers stay free of platform socket headers, and Windows initialization is handled through an RAII runtime wrapper.
