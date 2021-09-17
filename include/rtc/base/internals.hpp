#ifndef _RTC_BASE_INTERNALS_H_
#define _RTC_BASE_INTERNALS_H_

#include <stddef.h>
#include <cstdint>

namespace naivertc {

// IPv6 minimum guaranteed MTU
// IPv6 requires that every link in the internet have an MTU
// of 1280 octects or greater. On any link that cannot convey
// a 1280-octect packet in one piece, link-specific fragmentation
// and reassembly must be provieded at a layer below IPv6.
// See https://blog.csdn.net/dog250/article/details/88820733
constexpr size_t kDefaultMtuSize = 1280;

} // namespace naivertc

#endif