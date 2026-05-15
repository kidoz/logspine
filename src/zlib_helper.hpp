#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace logspine::detail {

#if defined(LOGSPINE_WITH_ZLIB)
bool gzip_compress(std::string_view input, std::string& output);
bool zlib_compress(std::string_view input, std::string& output);
#endif

} // namespace logspine::detail
