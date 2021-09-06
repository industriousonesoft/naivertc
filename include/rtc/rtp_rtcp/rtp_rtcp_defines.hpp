#ifndef _RTC_RTP_RTCP_DEFINES_H_
#define _RTC_RTP_RTCP_DEFINES_H_

#include "base/defines.hpp"
#include "rtc/base/internals.hpp"

namespace naivertc {

// RFC 3550 page 44, including null termination
constexpr size_t kRtcpCNameSize = 256;
// We assume ethernet
constexpr size_t kIpPacketSize = 1500;

// Transport header size in bytes 
// TODO: Update Transport overhead when transport router changed.
// constexpr size_t kTransportOverhead = 48;  UDP/IPv6
constexpr size_t kTransportOverhead = 28; // Assume UPD/IPv4 as a reasonable minimum.

constexpr int kVideoPayloadTypeFrequency = 90000;
// TODO(bugs.webrtc.org/6458): Remove this when all the depending projects are
// updated to correctly set rtp rate for RtcpSender.
constexpr int kBogusRtpRateForAudioRtcp = 8000;

constexpr size_t kRtpHeaderSize = 12;

constexpr size_t kRtxHeaderSize = 2;

constexpr size_t kRedForFecHeaderSize = 1;

static const int kMinSendSidePacketHistorySize = 600;

// 对于一个系统而言，需要定义一个epoch，所有的时间表示是基于这个基准点的，
// 对于linux而言，采用了和unix epoch一样的时间点：1970年1月1日0点0分0秒（UTC）。
// NTP协议使用的基准点是：1900年1月1日0点0分0秒（UTC）。
// GPS系统使用的基准点是：1980年1月6日0点0分0秒（UTC）。
// 每个系统都可以根据自己的逻辑定义自己epoch，例如unix epoch的基准点是因为unix操作系统是在1970年左右成型的。
// 详见 https://www.cnblogs.com/arnoldlu/p/7078179.html
enum class EpochType : unsigned long long {
    T1970 = 2208988800UL, // Number of seconds between 1970 and 1900
    T1900 = 0
};

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

// RtpPacket media types.
enum class RtpPacketType : size_t {
    AUDIO = 0,                     // Audio media packets.
    VIDEO = 1,                     // Video media packets.
    RETRANSMISSION = 2,            // Retransmission, sent as response to NACK.
    FEC = 3,                       // FEC (Forward Error Correction) packet.
    PADDING = 4                    // RTX or plain padding sent to maintain BEW.
};

// Rtcp packet type
enum class RtcpPacketType : uint32_t {
    REPORT = 0x0001,
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

// Rtx mode
enum class RtxMode : size_t {
    OFF = 0x0,
    RETRANSMITTED = 0x1,     // Only send retransmissions over RTX.
    REDUNDANT_PAYLOADS = 0x2  // Preventively send redundant payloads instead of padding.
};

} // namespace naivertc

#endif