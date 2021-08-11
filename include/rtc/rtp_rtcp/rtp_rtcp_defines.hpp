#ifndef _RTC_RTP_RTCP_DEFINES_H_
#define _RTC_RTP_RTCP_DEFINES_H_

#include "base/defines.hpp"

namespace naivertc {

// We assume ethernet
constexpr size_t kDefaultPacketSize = 1500;

// RtpPacket media types.
enum class RtpPacketMediaType : size_t {
    kAudio = 0,                     // Audio media packets.
    kVideo = 1,                     // Video media packets.
    kRetransmission = 2,            // Retransmission, sent as response to NACK.
    kForwardErrorCorrection = 3,    // FEC packet.
    kPadding = 4                    // RTX or plain padding sent to maintain BEW.
};
    
} // namespace naivertc


#endif