#ifndef _RTC_RTP_RTCP_DEFINES_H_
#define _RTC_RTP_RTCP_DEFINES_H_

#include "base/defines.hpp"
#include "rtc/base/internals.hpp"

namespace naivertc {

// RFC 3550 page 44, including null termination
constexpr size_t kRtcpCNameSize = 256;

constexpr int kVideoPayloadTypeFrequency = 90000;
// TODO(bugs.webrtc.org/6458): Remove this when all the depending projects are
// updated to correctly set rtp rate for RtcpSender.
constexpr int kBogusRtpRateForAudioRtcp = 8000;

constexpr size_t kRtpHeaderSize = 12;

constexpr size_t kRtxHeaderSize = 2;

constexpr size_t kRedForFecHeaderSize = 1;

static const int kMinSendSidePacketHistorySize = 600;

// RtpPacket media types.
enum class RtpPacketType : size_t {
    AUDIO = 0,                     // Audio media packets.
    VIDEO = 1,                     // Video media packets.
    RETRANSMISSION = 2,            // Retransmission, sent as response to NACK.
    FEC = 3,                       // FEC (Forward Error Correction) packet.
    PADDING = 4                    // RTX or plain padding sent to maintain BEW.
};

// Rtcp packet type
enum RtcpPacketType : uint32_t {
    RTCP_REPORT = 0x0001,
    SR = 0x0002,
    RR = 0x0004,
    SDES = 0x0008,
    BYE = 0x0010,
    PLI = 0x0020,
    NACK = 0x0040,
    FIR = 0x0080,
    TMMBR = 0x0100,
    TMMBN = 0x0200,
    SR_REQUEST = 0x0400,
    LOSS_NOTIFICATION = 0x2000,
    REMB = 0x10000,
    TRANSMISSION_TIME_OFFSET = 0x20000,
    XR_RECEIVER_REFERENCE_TIME = 0x40000,
    XR_DLRR_REPORT_BLOCK = 0x80000,
    TRANSPORT_FEEDBACK = 0x100000,
    XR_TARGET_BITRATE = 0x200000
};

// Rtcp mode
enum class RtcpMode {
    OFF,
    COMPOUND,
    // ReducedSize: See https://datatracker.ietf.org/doc/html/draft-ietf-rtcweb-rtp-usage-26#section-4.6
    REDUCED_SIZE
};

// Rtx mode
enum RtxMode {
    kRtxOff = 0x0,
    kRtxRetransmitted = 0x1,        // Only send retransmissions over RTX.
    kRtxRedundantPayloads = 0x2     // Preventively send redundant payloads
                                    // instead of padding.
};

} // namespace naivertc

#endif