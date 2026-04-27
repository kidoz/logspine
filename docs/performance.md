# Performance Notes

## Design Targets

- Disabled-level path: one atomic level load plus a branch.
- Enabled synchronous path: owned `log_event` construction followed by inline sink writes.
- Async path: bounded queue, explicit overflow policy, deterministic `flush()`.

## How to Run

```bash
meson setup build-bench -Dbuild_benchmarks=true
meson compile -C build-bench
./build-bench/bench_logging --iterations 200000 --threads 4 --queue-capacity 8192
```

The benchmark accepts:

- `--iterations N`
- `--threads N`
- `--queue-capacity N`

The benchmark executable prints simple key/value metrics:

```text
disabled_debug_ns_per_op=...
enabled_info_noop_ns_per_op=...
enabled_structured_ns_per_op=...
file_sink_ns_per_op=...
async_events_per_sec=...
async_dropped_events=...
```

## Caveats

- The current benchmark is intentionally dependency-free and uses `std::chrono`.
- Results are most useful for relative regressions on the same machine.
- Network sinks are not benchmarked yet because transport backoff and batching policies are still minimal.
- Catch2-backed tests and loopback network tests are correctness coverage, not performance measurements.
- Async throughput numbers depend materially on the chosen `--threads` and `--queue-capacity` values.
