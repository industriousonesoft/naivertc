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

// We assume ethernet
constexpr size_t kIpPacketSize = 1500;

// Transport header size in bytes 
// TODO: Update Transport overhead when transport router changed.
// constexpr size_t kTransportOverhead = 48;  UDP/IPv6
constexpr size_t kTransportOverhead = 28; // Assume UPD/IPv4 as a reasonable minimum.

} // namespace naivertc

#endif