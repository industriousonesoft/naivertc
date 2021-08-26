#ifndef _RTC_BASE_INTERNALS_H_
#define _RTC_BASE_INTERNALS_H_

#include <stddef.h>
#include <cstdint>

namespace naivertc {

constexpr size_t kDefaultSctpPort = 5000;
constexpr size_t kDefaultLocalMaxMessageSize = 256 * 1024;
// IPv6 minimum guaranteed MTU
// IPv6 requires that every link in the internet have an MTU
// of 1280 octects or greater. On any link that cannot convey
// a 1280-octect packet in one piece, link-specific fragmentation
// and reassembly must be provieded at a layer below IPv6.
// See https://blog.csdn.net/dog250/article/details/88820733
constexpr size_t kDefaultMtuSize = 1280;

using StreamId = uint16_t;
constexpr StreamId kStreamIdMaxValue = 65535;

}

#endif