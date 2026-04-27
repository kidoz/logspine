#include <logspine/registry.hpp>

#include <utility>

namespace logspine {

logger_registry::logger_registry(std::shared_ptr<dispatcher> dispatcher, level minimum_level)
    : dispatcher_(std::move(dispatcher)), minimum_level_(minimum_level) {}

std::shared_ptr<logger> logger_registry::get(std::string_view name) {
  std::scoped_lock lock(mutex_);

  const auto key = std::string(name);
  const auto found = loggers_.find(key);
  if (found != loggers_.end()) {
    return found->second;
  }

  auto created = std::make_shared<logger>(key, dispatcher_, minimum_level_.load(std::memory_order_relaxed));
  loggers_.emplace(key, created);
  return created;
}

void logger_registry::set_level(level value) {
  minimum_level_.store(value, std::memory_order_relaxed);

  std::scoped_lock lock(mutex_);
  for (auto& [name, current_logger] : loggers_) {
    static_cast<void>(name);
    current_logger->set_level(value);
  }
}

level logger_registry::level_threshold() const noexcept { return minimum_level_.load(std::memory_order_relaxed); }

void logger_registry::flush() { dispatcher_->flush(); }

}  // namespace logspine
