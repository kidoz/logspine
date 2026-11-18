# Run Benchmarks

## Configure and build

```bash
meson setup build-bench -Dbuild_benchmarks=true
meson compile -C build-bench
```

## Run the benchmark

```bash
./build-bench/bench_logging --iterations 200000 --threads 4 --queue-capacity 8192
```

The benchmark accepts:

- `--iterations N`
- `--threads N`
- `--queue-capacity N`

It prints one key/value metric per line:

```text
disabled_debug_ns_per_op=...
enabled_info_noop_ns_per_op=...
enabled_structured_ns_per_op=...
file_sink_ns_per_op=...
async_events_per_sec=...
async_dropped_events=...
```

Use the same machine and build configuration when comparing runs. See
[Performance](../explanation/performance.md) for what the numbers do and do not
measure.
