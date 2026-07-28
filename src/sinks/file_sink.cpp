#include <logspine/sinks/file_sink.hpp>

#include <stdexcept>
#include <system_error>
#include <utility>

#include "../zlib_helper.hpp"
#include <logspine/json.hpp>

namespace logspine::sinks {

file_sink::file_sink(file_sink_options options) : options_(std::move(options)) {
  open_file();
}

void file_sink::open_file() {
  auto open_mode = std::ios::out;
  open_mode |= options_.append ? std::ios::app : std::ios::trunc;
  stream_.open(options_.path, open_mode);
  if (!stream_.is_open()) {
    throw std::runtime_error("file_sink could not open output file");
  }
  stream_.exceptions(std::ios::badbit | std::ios::failbit);
}

void file_sink::rotate_if_needed() {
  if (options_.max_file_size == 0)
    return;

  const auto current_size = stream_.tellp();
  if (current_size < 0 || static_cast<std::size_t>(current_size) < options_.max_file_size)
    return;

  stream_.close();

  std::error_code ec;
  const bool compress = options_.compress_rotated;
#if !defined(LOGSPINE_WITH_ZLIB)
  if (compress) {
    // If zlib is not compiled in, ignore compression silent or log? We just ignore.
  }
#endif

  auto get_rotated_path = [this, compress](std::uint32_t index) {
    auto p = options_.path;
    p += "." + std::to_string(index);
#if defined(LOGSPINE_WITH_ZLIB)
    if (compress) {
      p += ".gz";
    }
#endif
    return p;
  };

  if (options_.max_files > 0) {
    for (std::uint32_t i = options_.max_files; i > 0; --i) {
      auto target = get_rotated_path(i);

      if (i == options_.max_files) {
        std::filesystem::remove(target, ec);
      }

      std::filesystem::path source;
      if (i > 1) {
        source = get_rotated_path(i - 1);
        if (std::filesystem::exists(source, ec)) {
          std::filesystem::rename(source, target, ec);
        }
      } else {
        source = options_.path;
        if (std::filesystem::exists(source, ec)) {
#if defined(LOGSPINE_WITH_ZLIB)
          if (compress) {
            std::error_code read_ec;
            auto sz = std::filesystem::file_size(source, read_ec);
            if (!read_ec) {
              std::string input(sz, '\0');
              std::ifstream in(source, std::ios::binary);
              in.read(input.data(), static_cast<std::streamsize>(sz));
              std::string compressed;
              if (::logspine::detail::gzip_compress(input, compressed)) {
                std::ofstream out(target, std::ios::binary | std::ios::trunc);
                out.write(compressed.data(), static_cast<std::streamsize>(compressed.size()));
              } else {
                std::filesystem::rename(source, target, ec); // Fallback to plain rename
              }
            } else {
              std::filesystem::rename(source, target, ec);
            }
          } else {
            std::filesystem::rename(source, target, ec);
          }
#else
          std::filesystem::rename(source, target, ec);
#endif
        }
      }
    }
  }

  options_.append = false;
  open_file();
}

void file_sink::write(const log_event& event) {
  if (!should_log(event))
    return;

  std::scoped_lock lock(mutex_);
  rotate_if_needed();

  if (formatter_) {
    std::string buffer;
    formatter_->format(event, buffer);
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
