#ifndef _RTC_RTP_RTCP_RTP_RTP_PARAMETERS_H_
#define _RTC_RTP_RTCP_RTP_RTP_PARAMETERS_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"
#include "rtc/rtp_rtcp/base/rtp_extensions.hpp"

#include <optional>

namespace naivertc {

constexpr size_t kDefaultMaxPacketSize = kIpPacketSize - kTransportOverhead;

// RtpParameters
struct RtpParameters {
    // SSRC used for the local media stream.
    uint32_t local_media_ssrc = 0;
    // Payload type used for media payload on the media stream.
    int media_payload_type = -1;
    // RTX payload type used for media payload on the RTX stream.
    std::optional<int> media_rtx_payload_type = -1;

    std::optional<uint32_t> rtx_send_ssrc = std::nullopt;

    // Corresponds to the SDP attribute extmap-allow-mixed
    bool extmap_allow_mixed = false;
    
    // The default time interval between RTCP report for video: 1000 ms
    // The default time interval between RTCP report for audio: 5000 ms
    size_t rtcp_report_interval_ms = 1000;

    size_t max_packet_size = kDefaultMaxPacketSize;

    // RtpHeaderExtension
    std::vector<RtpExtension> extensions;

    // NACK
    bool nack_enabled = false;

    // ULP_FEC + RED
    struct UlpFec {
        // Payload type used for ULP_FEC packets.
        int ulpfec_payload_type = -1;
        // Payload type used for RED packets.
        int red_payload_type = -1;
        // RTX payload type used for RED payload.
        std::optional<int> red_rtx_payload_type = -1;
    } ulpfec;

    // Flexfec: Separate stream
    struct Flexfec {
        // Payload type of FlexFEC. Set to -1 to disable sending FlexFEC.
        int payload_type = -1;
        // SSRC of FlexFEC stream.
        uint32_t ssrc = 0;
        // The media stream being protected by this FlexFEC stream.
        uint32_t protected_media_ssrc = 0;
    } flexfec;
};

// RtpSenderObservers
struct RtpSenderObservers {
    // RTP
    RtpSendDelayObserver* send_delay_observer = nullptr;
    RtpSendPacketObserver* send_packet_observer = nullptr;
    RtpSendBitratesObserver* send_bitrates_observer = nullptr;
    RtpTransportFeedbackObserver* rtp_transport_feedback_observer = nullptr;
    RtpStreamDataCountersObserver* stream_data_counters_observer = nullptr;

    // RTCP
    RtcpPacketTypeCounterObserver* packet_type_counter_observer = nullptr;
    RtcpIntraFrameObserver* intra_frame_observer = nullptr;
    RtcpLossNotificationObserver* loss_notification_observer = nullptr;
    RtcpBandwidthObserver* bandwidth_observer = nullptr;
    RtcpCnameObserver* cname_observer = nullptr;
    RtcpRttObserver* rtt_observer = nullptr;
    RtcpTransportFeedbackObserver* rtcp_transport_feedback_observer = nullptr;
};

} // namespace naivertc

#endif