#ifndef _RTC_RTP_RTCP_DEFINES_H_
#define _RTC_RTP_RTCP_DEFINES_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp_packet.hpp"

#include <list>
#include <vector>

namespace naivertc {

// We assume ethernet
constexpr size_t kIpPacketSize = 1500;

// RtpPacket media types.
enum class RtpPacketMediaType : size_t {
    kAudio = 0,                     // Audio media packets.
    kVideo = 1,                     // Video media packets.
    kRetransmission = 2,            // Retransmission, sent as response to NACK.
    kForwardErrorCorrection = 3,    // FEC packet.
    kPadding = 4                    // RTX or plain padding sent to maintain BEW.
};

enum RTCPPacketType : uint32_t {
    kRtcpReport = 0x0001,
    kRtcpSr = 0x0002,
    kRtcpRr = 0x0004,
    kRtcpSdes = 0x0008,
    kRtcpBye = 0x0010,
    kRtcpPli = 0x0020,
    kRtcpNack = 0x0040,
    kRtcpFir = 0x0080,
    kRtcpTmmbr = 0x0100,
    kRtcpTmmbn = 0x0200,
    kRtcpSrReq = 0x0400,
    kRtcpLossNotification = 0x2000,
    kRtcpRemb = 0x10000,
    kRtcpTransmissionTimeOffset = 0x20000,
    kRtcpXrReceiverReferenceTime = 0x40000,
    kRtcpXrDlrrReportBlock = 0x80000,
    kRtcpTransportFeedback = 0x100000,
    kRtcpXrTargetBitrate = 0x200000
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