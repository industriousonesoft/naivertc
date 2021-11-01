#ifndef _RTC_TRANSPORTS_SCTP_TRANSPORT_INTERNALS_H_
#define _RTC_TRANSPORTS_SCTP_TRANSPORT_INTERNALS_H_

#include "rtc/base/internals.hpp"

namespace naivertc {

constexpr size_t kDefaultSctpPort = 5000;
constexpr size_t kDefaultSctpMaxMessageSize = 256 * 1024;

// The number of outgoing streams that we'll negotiate. Since stream IDs (SIDs)
// are 0-based, the highest usable SID is 1023.
//
// RFC 8831 6.2. SCTP Association Management
// The number of streams negotiated during SCTP association setup SHOULD be 65535, which is the
// maximum number of streams that can be negotiated during the association setup.
// See https://tools.ietf.org/html/rfc8831#section-6.2
// However, we use 1024 in order to save memory. usrsctp allocates 104 bytes
// for each pair of incoming/outgoing streams (on a 64-bit system), so 65535
// streams would waste ~6MB.
//
// Note: "max" and "min" here are inclusive.
constexpr uint16_t kMaxSctpStreams = 1024;
constexpr uint16_t kMaxSctpStreamId = kMaxSctpStreams - 1;
constexpr uint16_t kMinSctpStreamId = 0;

} // namespace naivertc

#endif