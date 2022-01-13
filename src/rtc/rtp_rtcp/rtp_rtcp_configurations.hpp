#ifndef _RTC_RTP_RTCP_RTP_RTCP_INTERFACE_H_
#define _RTC_RTP_RTCP_RTP_RTCP_INTERFACE_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/rtp_rtcp/rtp_statistic_structs.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_interfaces.hpp"
#include "rtc/api/media_transport.hpp"

#include <optional>
#include <vector>
#include <memory>

namespace naivertc {

struct RTC_CPP_EXPORT RtpConfiguration {
    // True for a audio version of the RTP/RTCP module object false will create
    // a video version.
    bool audio = false;

    // Corresponds to extmap-allow-mixed in SDP negotiation.
    bool extmap_allow_mixed = false;

    // SSRCs for media and retransmission(RTX), respectively.
    // FlexFec SSRC is fetched from |flexfec_sender|.
    uint32_t local_media_ssrc = 0;
    std::optional<uint32_t> rtx_send_ssrc = std::nullopt;

    // If true, the RTP packet history will select RTX packets based on
    // heuristics such as send time, retransmission count etc, in order to
    // make padding potentially more useful.
    // If false, the last packet will always be picked. This may reduce CPU
    // overhead.
    bool enable_rtx_padding_prioritization = true;

    RtpSentStatisticsObserver* rtp_sent_statistics_observer = nullptr;

    Clock* clock;
    
    MediaTransport* send_transport;
};

struct RTC_CPP_EXPORT RtcpConfiguration {
    // True for a audio version of the RTP/RTCP module object false will create
    // a video version.
    bool audio = false;
    bool receiver_only = false;
  
    int rtcp_report_interval_ms = 0;
    
    // Corresponds to extmap-allow-mixed in SDP negotiation.
    bool extmap_allow_mixed = false;

    // SSRCs for media, retransmission(RTX) and FEC.
    uint32_t local_media_ssrc = 0;
    std::optional<uint32_t> rtx_send_ssrc = std::nullopt;
    std::optional<uint32_t> fec_ssrc = std::nullopt;

    Clock* clock;

    // Observers
    RtcpPacketTypeCounterObserver* packet_type_counter_observer = nullptr;
    RtcpIntraFrameObserver* intra_frame_observer = nullptr;
    RtcpLossNotificationObserver* loss_notification_observer = nullptr;
    RtcpBandwidthObserver* bandwidth_observer = nullptr;
    RtcpCnameObserver* cname_observer = nullptr;
    RtcpTransportFeedbackObserver* transport_feedback_observer = nullptr;

    RtcpReportBlockProvider* report_block_provider = nullptr;
};
        
} // namespace naivertc


#endif