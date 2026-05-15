#include <logspine/logger.hpp>

#include <chrono>
#include <type_traits>
#include <utility>

#include <logspine/log_event.hpp>
#include <logspine/mdc.hpp>

namespace logspine {

namespace {

[[nodiscard]] bool is_enabled_for(level minimum_level, level severity) noexcept {
  return minimum_level != level::off && severity != level::off &&
         static_cast<std::underlying_type_t<level>>(severity) >= static_cast<std::underlying_type_t<level>>(minimum_level);
}

}  // namespace

logger::logger(std::string name, std::shared_ptr<dispatcher> dispatcher, level minimum_level)
    : name_(std::move(name)), dispatcher_(std::move(dispatcher)), minimum_level_(minimum_level) {}

std::string_view logger::name() const noexcept { return name_; }

bool logger::enabled(level value) const noexcept { return is_enabled_for(minimum_level_.load(std::memory_order_relaxed), value); }

level logger::minimum_level() const noexcept { return minimum_level_.load(std::memory_order_relaxed); }

void logger::set_level(level value) noexcept { minimum_level_.store(value, std::memory_order_relaxed); }

void logger::flush() { dispatcher_->flush(); }

void logger::log(level severity, std::string_view message, source_location location) {
  if (!enabled(severity)) {
    return;
  }
  dispatch(severity, message, {}, location);
}

void logger::log(level severity, std::string_view message, std::initializer_list<field> fields, source_location location) {
  if (!enabled(severity)) {
    return;
  }
  dispatch(severity, message, std::vector<field>(fields), location);
}

void logger::log(level severity, std::string_view message, std::vector<field> fields, source_location location) {
  if (!enabled(severity)) {
    return;
  }
  dispatch(severity, message, std::move(fields), location);
}

void logger::trace(std::string_view message, source_location location) { log(level::trace, message, location); }
void logger::debug(std::string_view message, source_location location) { log(level::debug, message, location); }
void logger::info(std::string_view message, source_location location) { log(level::info, message, location); }
void logger::warn(std::string_view message, source_location location) { log(level::warn, message, location); }
void logger::error(std::string_view message, source_location location) { log(level::error, message, location); }
void logger::fatal(std::string_view message, source_location location) { log(level::fatal, message, location); }

void logger::trace(std::string_view message, std::initializer_list<field> fields, source_location location) {
  log(level::trace, message, fields, location);
}
void logger::debug(std::string_view message, std::initializer_list<field> fields, source_location location) {
  log(level::debug, message, fields, location);
}
void logger::info(std::string_view message, std::initializer_list<field> fields, source_location location) {
  log(level::info, message, fields, location);
}
void logger::warn(std::string_view message, std::initializer_list<field> fields, source_location location) {
  log(level::warn, message, fields, location);
}
void logger::error(std::string_view message, std::initializer_list<field> fields, source_location location) {
  log(level::error, message, fields, location);
}
void logger::fatal(std::string_view message, std::initializer_list<field> fields, source_location location) {
  log(level::fatal, message, fields, location);
}

void logger::dispatch(level severity, std::string_view message, std::vector<field> fields, source_location location) {
  constexpr std::size_t max_message_length = 32768;
  constexpr std::size_t max_fields_count = 64;

  log_event event;
  event.severity = severity;
  event.logger_name = name_;

  if (message.size() > max_message_length) {
    event.message = std::string(message.substr(0, max_message_length)) + "... [truncated]";
  } else {
    event.message = std::string(message);
  }

  auto mdc_fields = mdc::get_all();
  if (!mdc_fields.empty()) {
    fields.insert(fields.end(), mdc_fields.begin(), mdc_fields.end());
  }

  if (fields.size() > max_fields_count) {
    fields.erase(fields.begin() + max_fields_count, fields.end());
    fields.push_back(kv("_truncated_fields", true));
  }
  event.fields = std::move(fields);

  event.timestamp = std::chrono::system_clock::now();
  event.thread_id = std::this_thread::get_id();
  event.location = location;
  dispatcher_->dispatch(std::move(event));
}

}  // namespace logspine
