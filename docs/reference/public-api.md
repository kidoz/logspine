# Public API

Include `<logspine/logspine.hpp>` to get the complete public surface. Individual
headers under `include/logspine/` are also available and are self-contained.

## `logspine::level`

```cpp
enum class level : std::uint8_t {
  trace = 0,
  debug = 1,
  info = 2,
  warn = 3,
  error = 4,
  fatal = 5,
  off = 6,
};
```

| Function | Description |
|---|---|
| `to_string(level)` | Returns the lowercase level name. |
| `parse_level(std::string_view)` | Parses a level name, returning `std::nullopt` when unknown. |
| `to_gelf_level(level)` | Maps a level to the GELF/syslog severity number. |

## `logspine::field` and `kv()`

`field` owns a key and one typed value:

```cpp
using value_type = std::variant<std::nullptr_t, bool, std::int64_t,
                                std::uint64_t, double, std::string>;
```

| Member | Description |
|---|---|
| `field(std::string key, value_type value)` | Constructs a field. |
| `key()` | Returns the key. |
| `value()` | Returns the stored value. |

`kv(key, value)` overloads accept `nullptr`, `bool`, C strings, `std::string`,
`std::string_view`, signed and unsigned integers, and floating-point values.
The key and value are copied into the field.

## `logspine::log_event`

```cpp
struct log_event {
  level severity = level::info;
  std::string logger_name;
  std::string message;
  std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
  std::thread::id thread_id = std::this_thread::get_id();
  std::optional<source_location> location;
  std::vector<field> fields;
  std::optional<std::string> trace_id;
  std::optional<std::string> span_id;
  std::optional<std::string> correlation_id;
};
```

`source_location` is an alias for `std::source_location`. A queued event owns
all of its text and field values.

## `logspine::sink`, `formatter`, and `filter`

`formatter`:

```cpp
class formatter {
public:
  virtual ~formatter() = default;
  virtual void format(const log_event& event, std::string& dest) = 0;
};
```

`filter`:

```cpp
class filter {
public:
  virtual ~filter() = default;
  virtual bool accept(const log_event& event) = 0;
};
```

`sink`:

| Member | Description |
|---|---|
| `write(const log_event&)` | Pure virtual; writes one event. |
| `flush()` | Pure virtual; flushes buffered output. |
| `set_formatter(std::unique_ptr<formatter>)` | Replaces the sink formatter. |
| `set_filter(std::unique_ptr<filter>)` | Replaces the sink filter. |
| `should_log(const log_event&)` | Returns `true` unless a filter rejects the event. |
| `format_event(const log_event&, std::string&)` | Protected helper; returns `false` when no formatter is set. |

`logspine::noop_sink` implements `write()` and `flush()` as no-ops.

## `logspine::dispatcher`

```cpp
class dispatcher {
public:
  virtual ~dispatcher() = default;

  virtual void dispatch(log_event event) = 0;
  virtual void flush() = 0;
  [[nodiscard]] virtual std::uint64_t dropped_events() const noexcept = 0;
  [[nodiscard]] virtual std::uint64_t sink_failures() const noexcept = 0;
};
```

`dispatch` takes the event by value so ownership can move into a queue.

### `sync_dispatcher`

```cpp
explicit sync_dispatcher(std::vector<std::shared_ptr<sink>> sinks);
```

Writes to each sink inline. `dropped_events()` is always zero.

### `async_dispatcher`

```cpp
enum class overflow_policy {
  block,
  drop_newest,
  drop_oldest,
};

struct async_options {
  std::size_t queue_capacity = 8192;
  overflow_policy overflow = overflow_policy::drop_newest;
  std::size_t batch_size = 64;
  std::chrono::milliseconds block_retry_timeout = std::chrono::milliseconds(5000);
};

async_dispatcher(std::vector<std::shared_ptr<sink>> sinks,
                 async_options options = {});
```

| Member | Description |
|---|---|
| `dispatch(log_event)` | Enqueues an event for the worker thread. |
| `flush()` | Waits for all previously accepted events, then flushes sinks. |
| `dropped_events()` | Returns the number of events discarded by the overflow policy. |
| `sink_failures()` | Returns the number of exceptions caught from sinks. |

## `logspine::logger`

| Member | Description |
|---|---|
| `logger(std::string name, std::shared_ptr<dispatcher>, level)` | Constructs a logger. |
| `name()` | Returns the logger name. |
| `enabled(level)` | Returns whether a level passes the current threshold. |
| `minimum_level()` | Returns the current threshold. |
| `set_level(level)` | Changes the threshold atomically. |
| `flush()` | Flushes the dispatcher. |
| `log(level, message, ...)` | Logs with no fields, an initializer list, or a vector of fields. |
| `trace`/`debug`/`info`/`warn`/`error`/`fatal` | Convenience overloads with and without fields. |

## `logspine::logger_registry`

| Member | Description |
|---|---|
| `logger_registry(std::shared_ptr<dispatcher>, level)` | Constructs a registry with an initial minimum level. |
| `get(std::string_view name)` | Returns a stable `std::shared_ptr<logger>` for the name. |
| `set_level(level)` | Updates the registry default and every existing logger. |
| `level_threshold()` | Returns the registry default level. |
| `flush()` | Flushes the dispatcher. |

`get()` is thread-safe and returns the same logger instance for repeated calls
with the same name.

## `logspine::mdc` and `logspine::scoped_mdc`

| Member | Description |
|---|---|
| `mdc::put(key, value)` | Adds or replaces a thread-local field. |
| `mdc::remove(key)` | Removes a thread-local field. |
| `mdc::clear()` | Clears all thread-local fields. |
| `mdc::get_all()` | Returns the current thread's fields. |
| `scoped_mdc(key, value)` | Sets a field for the lifetime of the object. |

## Logging macros

| Macro | Severity |
|---|---|
| `LOGSPINE_TRACE(logger, message, ...)` | `level::trace` |
| `LOGSPINE_DEBUG(logger, message, ...)` | `level::debug` |
| `LOGSPINE_INFO(logger, message, ...)` | `level::info` |
| `LOGSPINE_WARN(logger, message, ...)` | `level::warn` |
| `LOGSPINE_ERROR(logger, message, ...)` | `level::error` |
| `LOGSPINE_FATAL(logger, message, ...)` | `level::fatal` |

The first argument must be a `logger` lvalue. Additional arguments may be
`field` values or arguments for a `std::format` format string. The macro checks
the level before evaluating the message and field arguments.

## JSON helpers

| Function | Description |
|---|---|
| `append_json_escaped(std::string_view, std::string&)` | Appends an escaped JSON string body. |
| `to_json(const field::value_type&)` | Serializes one field value. |
| `to_json(const log_event&)` | Serializes an event without a trailing newline. |
| `to_json_lines_record(const log_event&)` | Serializes an event and appends `\n`. |
| `format_human_readable(const log_event&)` | Formats an event for human-readable output. |

## Configuration constant

`logspine::network_enabled` is `true` when the library was built with
`-Dnetwork=true` and `false` otherwise.
