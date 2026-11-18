# Performance

## Design targets

- Disabled-level path: one atomic level load plus a branch.
- Enabled synchronous path: owned `log_event` construction followed by inline
  sink writes.
- Async path: bounded queue, explicit overflow policy, deterministic `flush()`.

## What the benchmark measures

`bench_logging` reports:

- `disabled_debug_ns_per_op` — cost of a disabled debug call.
- `enabled_info_noop_ns_per_op` — enabled logging into a no-op sink.
- `enabled_structured_ns_per_op` — enabled logging with structured fields.
- `file_sink_ns_per_op` — file sink write cost.
- `async_events_per_sec` — async throughput.
- `async_dropped_events` — events discarded under the selected overflow policy.

See [Run benchmarks](../how-to/run-benchmarks.md) for the command line.

## Caveats

- The benchmark is dependency-free and uses `std::chrono`.
- Results are most useful for relative regressions on the same machine and build
  configuration.
- Network sinks are not benchmarked. Transport backoff and batching policies are
  still minimal.
- Catch2 tests and loopback network tests are correctness coverage, not
  performance measurements.
- Async throughput depends materially on the chosen `--threads` and
  `--queue-capacity` values.
