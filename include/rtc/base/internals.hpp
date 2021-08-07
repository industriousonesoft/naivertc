#ifndef _RTC_BASE_INTERNALS_H_
#define _RTC_BASE_INTERNALS_H_

#include <stddef.h>
#include <cstdint>

namespace naivertc {

const size_t kDefaultSctpPort = 5000;
const size_t kDefaultLocalMaxMessageSize = 256 * 1024;
// IPv6 minimum guaranteed MTU
const size_t kDefaultMtuSize = 1280;

using StreamId = uint16_t;
const StreamId kStreamIdMaxValue = 65535;

}

#endif