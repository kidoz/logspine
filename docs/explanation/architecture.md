# Architecture

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

The facade layer owns ergonomics and disabled-level fast paths. Dispatchers own
concurrency and failure isolation. Sinks own output formatting and transport
details. Sinks do not reach back into registry or logger state.

## Responsibilities

- `logger_registry` owns stable named `logger` instances and a shared
  dispatcher.
- `logger` gates disabled levels, builds `log_event` values, and forwards
  accepted events to the dispatcher.
- `dispatcher` serializes access to one or more sinks.
- `sink` formats and writes events. A sink can also reject events through a
  filter and replace its output through a formatter.

See [Public API](../reference/public-api.md) for the type surface and
[Sinks and options](../reference/sinks.md) for per-sink configuration.

## Ownership and lifetimes

- Sinks are shared with `std::shared_ptr<sink>`.
- Dispatchers are shared with `std::shared_ptr<dispatcher>`.
- `logger_registry` holds the dispatcher and owns stable
  `std::shared_ptr<logger>` instances keyed by name.
- `logger` stores an atomic minimum level and a shared dispatcher reference.
- `log_event` owns message text and structured field values before entering a
  queue. No queued event stores `std::string_view`.

Create the registry at an explicit application entry point, such as inside
`main()`. Logging during static destruction is intentionally unsupported.

## Thread safety

- `logger` and `logger_registry` are safe for concurrent use.
- `logger::enabled()` is a relaxed atomic load plus comparison.
- `sync_dispatcher` serializes sink access with a mutex.
- `async_dispatcher` serializes sink writes on a dedicated worker thread.
- Built-in sinks snapshot filters and formatters per invocation and serialize
  calls to them. Replacing a callback lets an invocation that already holds a
  snapshot finish safely.
- Sink exceptions are caught at dispatcher boundaries and counted through
  `sink_failures()`. Network sinks also expose sink-local transport counters.

## Async behavior

`async_dispatcher` uses a bounded queue and a single worker thread.

- `overflow_policy::block`: the producer waits for queue capacity.
- `overflow_policy::drop_newest`: the new event is dropped.
- `overflow_policy::drop_oldest`: the oldest queued event is removed to make
  room.

`flush()` inserts an ordered flush request into the queue, waits for the worker
to process all prior events, then flushes the sinks. The destructor marks the
dispatcher as stopping, drains remaining work, flushes the sinks, and joins the
worker thread.

The worker processes accepted events in batches up to `batch_size`. The queue is
a bounded lock-free multi-producer, single-consumer queue.

## Failure isolation

A throwing sink must not take down the dispatcher or the caller. Both
dispatchers catch sink exceptions and increment the failure count. Transport
failures are additionally reported through sink-local state such as
`statistics()` and `last_error_message()`.

## Network and transport limits

- Network transports use plaintext. Use a trusted network or a local collector
  that secures onward transport.
- TCP JSON Lines and GELF UDP report local socket outcomes. OTLP/HTTP also
  checks the collector acknowledgement.
- Elasticsearch HTTP bulk upload is intentionally out of scope. The
  Elasticsearch sink writes offline NDJSON for later submission.
- TLS, authentication, and richer reconnect policies are not implemented yet.
