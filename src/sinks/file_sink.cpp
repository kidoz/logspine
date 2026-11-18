#include <logspine/sinks/file_sink.hpp>

#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "../zlib_helper.hpp"
#include <logspine/json.hpp>

namespace logspine::sinks {

#ifdef LOGSPINE_WITH_ZLIB
void file_sink::write_compressed_archive(const std::filesystem::path& archive) {
  const auto size = std::filesystem::file_size(options_.path);
  if (size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()) ||
      size > std::string{}.max_size()) {
    throw std::length_error("log file too large to compress");
  }
  std::string input(static_cast<std::size_t>(size), '\0');
  std::ifstream in;
  in.exceptions(std::ios::badbit | std::ios::failbit);
  in.open(options_.path, std::ios::binary);
  in.read(input.data(), static_cast<std::streamsize>(size));
  in.close();

  std::string compressed;
  if (!::logspine::detail::gzip_compress(input, compressed) ||
      compressed.size() > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
    throw std::runtime_error("file_sink could not compress rotated log");
  }

  std::ofstream out;
  out.exceptions(std::ios::badbit | std::ios::failbit);
  out.open(archive, std::ios::binary | std::ios::noreplace);
  try {
    out.write(compressed.data(), static_cast<std::streamsize>(compressed.size()));
    out.close();
  } catch (...) {
    if (out.is_open()) {
      out.exceptions(std::ios::goodbit);
      out.close();
    }
    std::error_code ignored;
    std::filesystem::remove(archive, ignored);
    throw;
  }
}
#endif

file_sink::file_sink(file_sink_options options) : options_(std::move(options)) {
  open_file(options_.append);
}

void file_sink::open_file(bool append) {
  auto open_mode = std::ios::out;
  open_mode |= append ? std::ios::app : std::ios::trunc;
  stream_.open(options_.path, open_mode);
  if (!stream_.is_open()) {
    throw std::runtime_error("file_sink could not open output file");
  }
  stream_.exceptions(std::ios::badbit | std::ios::failbit);
}

void file_sink::rotate_if_needed() {
  if (options_.max_file_size == 0) {
    return;
  }

  const auto current_size = static_cast<std::streamoff>(stream_.tellp());
  if (current_size < 0) {
    throw std::runtime_error("file_sink could not determine log size");
  }
  if (std::cmp_less(current_size, options_.max_file_size)) {
    return;
  }

  try {
    stream_.close();

    if (options_.max_files != 0) {
      const auto rotated_path = [this](std::uint32_t index) {
        auto path = options_.path;
        path += "." + std::to_string(index);
#ifdef LOGSPINE_WITH_ZLIB
        if (options_.compress_rotated) {
          path += ".gz";
        }
#endif
        return path;
      };

      std::filesystem::remove(rotated_path(options_.max_files));
      for (auto index = options_.max_files; index > 1; --index) {
        const auto source = rotated_path(index - 1);
        if (std::filesystem::exists(std::filesystem::symlink_status(source))) {
          std::filesystem::rename(source, rotated_path(index));
        }
      }

      const auto archive = rotated_path(1);
#ifdef LOGSPINE_WITH_ZLIB
      if (options_.compress_rotated) {
        write_compressed_archive(archive);
      } else {
        std::filesystem::rename(options_.path, archive);
      }
#else
      std::filesystem::rename(options_.path, archive);
#endif
    }

    open_file(false);
  } catch (...) {
    if (!stream_.is_open()) {
      stream_.clear();
      open_file(true);
    }
    throw;
  }
}

void file_sink::write(const log_event& event) {
  if (!should_log(event)) {
    return;
  }

  std::scoped_lock lock(mutex_);
  rotate_if_needed();

  std::string buffer;
  if (format_event(event, buffer)) {
    stream_ << buffer;
  } else if (options_.format == sink_format::json_lines) {
    stream_ << to_json_lines_record(event);
  } else {
    stream_ << format_human_readable(event) << '\n';
  }
}

void file_sink::flush() {
  std::scoped_lock lock(mutex_);
  stream_.flush();
}

} // namespace logspine::sinks
