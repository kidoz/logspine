#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <logspine/dispatcher.hpp>
#include <logspine/level.hpp>
#include <logspine/logger.hpp>

namespace logspine {

class logger_registry {
 public:
  logger_registry(std::shared_ptr<dispatcher> dispatcher, level minimum_level);

  [[nodiscard]] std::shared_ptr<logger> get(std::string_view name);
  void set_level(level value);
  [[nodiscard]] level level_threshold() const noexcept;
  void flush();

 private:
  std::shared_ptr<dispatcher> dispatcher_;
  std::atomic<level> minimum_level_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<logger>> loggers_;
};

}  // namespace logspine
