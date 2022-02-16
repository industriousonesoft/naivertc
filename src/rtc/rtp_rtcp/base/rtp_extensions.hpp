#ifndef _RTC_RTP_RTCP_BASE_RTP_EXTENSIONS_H_
#define _RTC_RTP_RTCP_BASE_RTP_EXTENSIONS_H_

#include "base/defines.hpp"

namespace naivertc {

// Rtp header extensions type
enum class RtpExtensionType : int {
    NONE = 0,
    TRANSMISSTION_TIME_OFFSET,
    ABSOLUTE_SEND_TIME,
    ABSOLUTE_CAPTURE_TIME,
    TRANSPORT_SEQUENCE_NUMBER,
    PLAYOUT_DELAY_LIMITS,
    RTP_STREAM_ID,
    REPAIRED_RTP_STREAM_ID,
    MID,
    NUMBER_OF_EXTENSIONS
};

// RtpExtension
struct RTC_CPP_EXPORT RtpExtension {

static constexpr int kInvalidId = 0;
static constexpr int kMinId = 1;
static constexpr int kMaxId = 255;
static constexpr int kMaxValueSize = 255;
static constexpr int kOneByteHeaderExtensionMaxId = 14;
static constexpr int kOneByteHeaderExtensionReservedId = 15; // The maximum value of 4 bits
static constexpr int kOneByteHeaderExtensionMaxValueSize = 16; // The maximum value of 4 bits + 1

};


} // namespace naivertc

#endif