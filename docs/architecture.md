# LogSpine Architecture

## Layers

```text
application code
  ↓
logger facade / registry
  ↓
dispatcher interface
  ↓
sync or async dispatcher
  ↓
sink interface
  ↓
formatters / serializers / filesystem / sockets
```

The facade layer owns ergonomics and disabled-level fast paths. Dispatchers own concurrency and failure isolation. Sinks own output formatting and transport details. Sinks do not reach back into registry or logger state.

## Public API

- `level`: severity enum plus text parsing and GELF mapping.
- `field` and `kv()`: typed structured properties copied into owned event storage.
- `log_event`: immutable-by-convention payload with timestamp, logger name, message, thread id, optional source location, optional correlation ids, and structured fields.
- `sink`: `write(const log_event&)` and `flush()`.
- `dispatcher`: `dispatch(log_event)` by value for safe move into queue ownership.
- `logger`: level gate plus convenience methods and macro-friendly overloads.
- `logger_registry`: thread-safe stable logger lookup over a shared dispatcher.

Headers under `include/logspine/` are public. Serialization, queueing, and transport implementations live under `src/`.

## Ownership and Lifetimes

- Sinks are shared with `std::shared_ptr<sink>`.
- Dispatchers are shared with `std::shared_ptr<dispatcher>`.
- `logger_registry` holds the dispatcher and owns stable `std::shared_ptr<logger>` instances keyed by name.
- `logger` stores an atomic minimum level and a shared dispatcher reference.
- `log_event` owns message text and structured field values before entering a queue. No queued event stores `std::string_view`.

Logging during static destruction is intentionally unsupported as a design target. The intended lifecycle is registry creation inside `main()` or another explicit application entry point.

## Thread Safety

- `logger` and `logger_registry` are safe for concurrent use.
- `logger::enabled()` is a relaxed atomic load plus comparison.
- `sync_dispatcher` serializes sink access with a mutex.
- `async_dispatcher` serializes sink writes on a dedicated worker thread.
- Sink exceptions are caught at dispatcher boundaries and counted through `sink_failures()`. Network sinks also expose sink-local write and reconnect counters for transport-level observability.

## Async Behavior

`async_dispatcher` uses a bounded queue and a single worker thread.

- `overflow_policy::block`: producer waits for queue capacity.
- `overflow_policy::drop_newest`: the new event is dropped.
- `overflow_policy::drop_oldest`: the oldest queued event is removed to make room.

`flush()` inserts an ordered flush request into the queue, waits for the worker to process all prior events, then flushes sinks. The destructor marks the dispatcher as stopping, drains remaining work, flushes sinks, and joins the worker thread.

## Sink Categories

- Implemented now: console, file, Elasticsearch bulk NDJSON, TCP JSON Lines, Graylog GELF UDP.
- Future work: richer reconnect policy, TLS/auth, compression, GELF chunking, OTLP logs.

## Risks and Hardening Items

- Network sinks need per-destination reconnect, backoff, and error accounting.
- GELF production use still needs documented chunking/compression behavior.
- Elasticsearch HTTP bulk upload is intentionally out of scope for the first pass; the current sink writes offline NDJSON for later submission.
