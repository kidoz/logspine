#include "zlib_helper.hpp"

#if defined(LOGSPINE_WITH_ZLIB)

#include <zlib.h>
#include <stdexcept>

namespace logspine::detail {

namespace {

bool compress_impl(std::string_view input, std::string& output, int window_bits) {
  z_stream zs{};
  if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
    return false;
  }

  zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
  zs.avail_in = static_cast<uInt>(input.size());

  int ret;
  char outbuffer[32768];

  do {
    zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
    zs.avail_out = sizeof(outbuffer);

    ret = deflate(&zs, Z_FINISH);

    if (output.size() < zs.total_out) {
      output.append(outbuffer, zs.total_out - output.size());
    }
  } while (ret == Z_OK);

  deflateEnd(&zs);

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
