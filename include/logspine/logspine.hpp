#pragma once

#include <logspine/async_dispatcher.hpp>
#include <logspine/config.hpp>
#include <logspine/dispatcher.hpp>
#include <logspine/field.hpp>
#include <logspine/filter.hpp>
#include <logspine/formatter.hpp>
#include <logspine/json.hpp>
#include <logspine/level.hpp>
#include <logspine/log_event.hpp>
#include <logspine/logger.hpp>
#include <logspine/mdc.hpp>
#include <logspine/noop_sink.hpp>
#include <logspine/registry.hpp>
#include <logspine/sink.hpp>
#include <logspine/source_location.hpp>
#include <logspine/sync_dispatcher.hpp>
#include <logspine/sinks/console_sink.hpp>
#include <logspine/sinks/elastic_bulk_file_sink.hpp>
#include <logspine/sinks/file_sink.hpp>
#include <logspine/sinks/gelf_udp_sink.hpp>
#include <logspine/sinks/network_sink_statistics.hpp>
#include <logspine/sinks/tcp_json_lines_sink.hpp>

#define LOGSPINE_DETAIL_LOG(LOGSPINE_LOGGER, LOGSPINE_LEVEL, LOGSPINE_MESSAGE, ...)                           \
  do {                                                                                                         \
    auto& logspine_logger__ = (LOGSPINE_LOGGER);                                                               \
    if (logspine_logger__.enabled((LOGSPINE_LEVEL))) {                                                         \
      logspine_logger__.log((LOGSPINE_LEVEL), (LOGSPINE_MESSAGE), {__VA_ARGS__}, ::logspine::source_location::current()); \
    }                                                                                                          \
  } while (false)

#define LOGSPINE_TRACE(LOGSPINE_LOGGER, LOGSPINE_MESSAGE, ...) \
  LOGSPINE_DETAIL_LOG((LOGSPINE_LOGGER), ::logspine::level::trace, (LOGSPINE_MESSAGE) __VA_OPT__(, ) __VA_ARGS__)

#define LOGSPINE_DEBUG(LOGSPINE_LOGGER, LOGSPINE_MESSAGE, ...) \
  LOGSPINE_DETAIL_LOG((LOGSPINE_LOGGER), ::logspine::level::debug, (LOGSPINE_MESSAGE) __VA_OPT__(, ) __VA_ARGS__)

#define LOGSPINE_INFO(LOGSPINE_LOGGER, LOGSPINE_MESSAGE, ...) \
  LOGSPINE_DETAIL_LOG((LOGSPINE_LOGGER), ::logspine::level::info, (LOGSPINE_MESSAGE) __VA_OPT__(, ) __VA_ARGS__)

#define LOGSPINE_WARN(LOGSPINE_LOGGER, LOGSPINE_MESSAGE, ...) \
  LOGSPINE_DETAIL_LOG((LOGSPINE_LOGGER), ::logspine::level::warn, (LOGSPINE_MESSAGE) __VA_OPT__(, ) __VA_ARGS__)

#define LOGSPINE_ERROR(LOGSPINE_LOGGER, LOGSPINE_MESSAGE, ...) \
  LOGSPINE_DETAIL_LOG((LOGSPINE_LOGGER), ::logspine::level::error, (LOGSPINE_MESSAGE) __VA_OPT__(, ) __VA_ARGS__)

#define LOGSPINE_FATAL(LOGSPINE_LOGGER, LOGSPINE_MESSAGE, ...) \
  LOGSPINE_DETAIL_LOG((LOGSPINE_LOGGER), ::logspine::level::fatal, (LOGSPINE_MESSAGE) __VA_OPT__(, ) __VA_ARGS__)
