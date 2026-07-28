#pragma once

#ifdef LOGSPINE_WITH_NETWORK
#define LOGSPINE_CONFIG_NETWORK 1
#else
#define LOGSPINE_CONFIG_NETWORK 0
#endif

namespace logspine {

inline constexpr bool network_enabled = LOGSPINE_CONFIG_NETWORK != 0;

} // namespace logspine
