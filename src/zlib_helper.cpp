#include "zlib_helper.hpp"

#ifdef LOGSPINE_WITH_ZLIB

#include <array>
#include <limits>
#include <zlib.h>

namespace logspine::detail {

namespace {

bool compress_impl(std::string_view input, std::string& output, int window_bits) {
  if (input.size() > std::numeric_limits<uInt>::max()) {
    return false;
  }
  z_stream zs{};
  if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
    return false;
  }

  struct end_deflate {
    z_stream& stream;
    ~end_deflate() {
      deflateEnd(&stream);
    }
  } cleanup{zs};

  zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
  zs.avail_in = static_cast<uInt>(input.size());

  int ret;
  std::array<char, 32768> buffer{};

  do {
    zs.next_out = reinterpret_cast<Bytef*>(buffer.data());
    zs.avail_out = buffer.size();

    ret = deflate(&zs, Z_FINISH);

    output.append(buffer.data(), buffer.size() - zs.avail_out);
  } while (ret == Z_OK);

  return ret == Z_STREAM_END;
}

} // namespace

bool gzip_compress(std::string_view input, std::string& output) {
  // 15 + 16 for gzip
  return compress_impl(input, output, 15 + 16);
}

bool zlib_compress(std::string_view input, std::string& output) {
  // 15 for zlib
  return compress_impl(input, output, 15);
}

} // namespace logspine::detail

#endif
