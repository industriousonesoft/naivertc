#ifndef _RTC_RTP_RTCP_DEFINES_H_
#define _RTC_RTP_RTCP_DEFINES_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"

#include <list>
#include <vector>
#include <optional>

namespace naivertc {

// RFC 3550 page 44, including null termination
constexpr size_t kRtcpCNameSize = 256;
// We assume ethernet
constexpr size_t kIpPacketSize = 1500;

const int kVideoPayloadTypeFrequency = 90000;
// TODO(bugs.webrtc.org/6458): Remove this when all the depending projects are
// updated to correctly set rtp rate for RtcpSender.
const int kBogusRtpRateForAudioRtcp = 8000;

const size_t kRtxHeaderSize = 2;

// Rtp header extensions type
enum class RtpExtensionType : int {
    NONE = 0,
    TRANSMISSTION_TIME_OFFSET,
    ABSOLUTE_SEND_TIME,
    ABSOLUTE_CAPTURE_TIME,
    TRANSPORT_SEQUENCE_NUMBER,
    PLAYOUT_DELAY,
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

// RTCPReportBlock
struct RTCPReportBlock {
  RTCPReportBlock()
      : sender_ssrc(0),
        source_ssrc(0),
        fraction_lost(0),
        packets_lost(0),
        extended_highest_sequence_number(0),
        jitter(0),
        last_sender_report_timestamp(0),
        delay_since_last_sender_report(0) {}

  RTCPReportBlock(uint32_t sender_ssrc,
                  uint32_t source_ssrc,
                  uint8_t fraction_lost,
                  int32_t packets_lost,
                  uint32_t extended_highest_sequence_number,
                  uint32_t jitter,
                  uint32_t last_sender_report_timestamp,
                  uint32_t delay_since_last_sender_report)
      : sender_ssrc(sender_ssrc),
        source_ssrc(source_ssrc),
        fraction_lost(fraction_lost),
        packets_lost(packets_lost),
        extended_highest_sequence_number(extended_highest_sequence_number),
        jitter(jitter),
        last_sender_report_timestamp(last_sender_report_timestamp),
        delay_since_last_sender_report(delay_since_last_sender_report) {}

    // Fields as described by RFC 3550 6.4.2.
    uint32_t sender_ssrc;  // SSRC of sender of this report.
    uint32_t source_ssrc;  // SSRC of the RTP packet sender.
    uint8_t fraction_lost;
    int32_t packets_lost;  // 24 bits valid.
    uint32_t extended_highest_sequence_number;
    uint32_t jitter;
    uint32_t last_sender_report_timestamp;
    uint32_t delay_since_last_sender_report;
};

typedef std::list<RTCPReportBlock> ReportBlockList;

// RtpState
struct RTC_CPP_EXPORT RtpState {
    RtpState()
        : sequence_num(0),
          start_timestamp(0),
          timestamp(0),
          capture_time_ms(-1),
          last_timestamp_time_ms(-1),
          ssrc_has_acked(false) {}

    uint16_t sequence_num;
    uint32_t start_timestamp;
    uint32_t timestamp;
    int64_t capture_time_ms;
    int64_t last_timestamp_time_ms;
    bool ssrc_has_acked;
};

// The Absolute Capture Time extension is used to stamp RTP packets with a NTP
// timestamp showing when the first audio or video frame in a packet was
// originally captured. The intent of this extension is to provide a way to
// accomplish audio-to-video synchronization when RTCP-terminating intermediate
// systems (e.g. mixers) are involved. See:
// http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time
struct AbsoluteCaptureTime {
    // Absolute capture timestamp is the NTP timestamp of when the first frame in
    // a packet was originally captured. This timestamp MUST be based on the same
    // clock as the clock used to generate NTP timestamps for RTCP sender reports
    // on the capture system.
    //
    // It’s not always possible to do an NTP clock readout at the exact moment of
    // when a media frame is captured. A capture system MAY postpone the readout
    // until a more convenient time. A capture system SHOULD have known delays
    // (e.g. from hardware buffers) subtracted from the readout to make the final
    // timestamp as close to the actual capture time as possible.
    //
    // This field is encoded as a 64-bit unsigned fixed-point number with the high
    // 32 bits for the timestamp in seconds and low 32 bits for the fractional
    // part. This is also known as the UQ32.32 format and is what the RTP
    // specification defines as the canonical format to represent NTP timestamps.
    uint64_t absolute_capture_timestamp;

    // Estimated capture clock offset is the sender’s estimate of the offset
    // between its own NTP clock and the capture system’s NTP clock. The sender is
    // here defined as the system that owns the NTP clock used to generate the NTP
    // timestamps for the RTCP sender reports on this stream. The sender system is
    // typically either the capture system or a mixer.
    //
    // This field is encoded as a 64-bit two’s complement signed fixed-point
    // number with the high 32 bits for the seconds and low 32 bits for the
    // fractional part. It’s intended to make it easy for a receiver, that knows
    // how to estimate the sender system’s NTP clock, to also estimate the capture
    // system’s NTP clock:
    //
    //   Capture NTP Clock = Sender NTP Clock + Capture Clock Offset
    std::optional<int64_t> estimated_capture_clock_offset;
};

// Minimum and maximum playout delay values from capture to render.
// These are best effort values.
//
// A value < 0 indicates no change from previous valid value.
//
// min = max = 0 indicates that the receiver should try and render
// frame as soon as possible.
//
// min = x, max = y indicates that the receiver is free to adapt
// in the range (x, y) based on network jitter.
struct VideoPlayoutDelay {
    VideoPlayoutDelay() = default;
    VideoPlayoutDelay(int min_ms, int max_ms) : min_ms(min_ms), max_ms(max_ms) {}
    int min_ms = -1;
    int max_ms = -1;

    bool operator==(const VideoPlayoutDelay& rhs) const {
        return min_ms == rhs.min_ms && max_ms == rhs.max_ms;
    }
};

// Observer
// RtcpIntraFrameObserver
class RTC_CPP_EXPORT RtcpIntraFrameObserver {
public:
    virtual ~RtcpIntraFrameObserver() {}

    virtual void OnReceivedIntraFrameRequest(uint32_t ssrc) = 0;
};

// RtcpLossNotificationObserver
class RTC_CPP_EXPORT RtcpLossNotificationObserver {
public:
    virtual ~RtcpLossNotificationObserver() = default;

    virtual void OnReceivedLossNotification(uint32_t ssrc,
                                            uint16_t seq_num_of_last_decodable,
                                            uint16_t seq_num_of_last_received,
                                            bool decodability_flag) = 0;
};

// RtcpBandwidthObserver
class RTC_CPP_EXPORT RtcpBandwidthObserver {
public:
    // REMB or TMMBR
    virtual void OnReceivedEstimatedBitrate(uint32_t bitrate) = 0;

    virtual void OnReceivedRtcpReceiverReport(
        const ReportBlockList& report_blocks,
        int64_t rtt,
        int64_t now_ms) = 0;

    virtual ~RtcpBandwidthObserver() {}
};

// Interface for PacketRouter to send rtcp feedback on behalf of
// congestion controller.
// TODO(bugs.webrtc.org/8239): Remove and use RtcpTransceiver directly
// when RtcpTransceiver always present in rtp transport.
class RTC_CPP_EXPORT RtcpFeedbackSenderInterface {
public:
    virtual ~RtcpFeedbackSenderInterface() = default;
    virtual void SendCombinedRtcpPacket(std::vector<std::unique_ptr<RtcpPacket>> rtcp_packets) = 0;
    virtual void SetRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs) = 0;
    virtual void UnsetRemb() = 0;
};
    
} // namespace naivertc

#endif