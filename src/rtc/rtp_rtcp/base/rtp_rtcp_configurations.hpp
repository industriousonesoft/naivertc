#ifndef _RTC_RTP_RTCP_RTP_RTCP_CONFIGURATION_H_
#define _RTC_RTP_RTCP_RTP_RTCP_CONFIGURATION_H_

#include "base/defines.hpp"
#include "rtc/base/time/clock.hpp"
#include "rtc/rtp_rtcp/base/rtp_statistic_structs.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_interfaces.hpp"
#include "rtc/transports/rtc_transport_media.hpp"

#include <optional>
#include <vector>
#include <memory>

namespace naivertc {

class FecGenerator;

struct RtpConfiguration {
    // True for a audio version of the RTP/RTCP module object false will create
    // a video version.
    bool audio = false;

    // Corresponds to extmap-allow-mixed in SDP negotiation.
    bool extmap_allow_mixed = false;

    // Indicates to estimate the send-side bandwidth with overhead or not.
    bool send_side_bwe_with_overhead = false;

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

    // Too low factor means RTX payload padding is rarely used and ineffective.
    // Too high means we risk interrupting regular media packets.
    // In practice, 3x seems to yield reasonable results.
    double max_padding_size_factor = 3.0;

    // If true, the RTP sender will always annotate outgoing packets with
    // MID and RID header extensions, if provided and negotiated.
    // If false, the RTP sender will stop sending MID and RID header extensions,
    // when it knows that the receiver is ready to demux based on SSRC. This is
    // done by RTCP RR acking.
    bool always_send_mid_and_rid = false;

    Clock* clock;
    
    RtcMediaTransport* send_transport = nullptr;
    FecGenerator* fec_generator = nullptr;
    RtpPacketSender* paced_sender = nullptr;

    RtpSendDelayObserver* send_delay_observer = nullptr;
    RtpSendPacketObserver* send_packet_observer = nullptr;
    RtpSendBitratesObserver* send_bitrates_observer = nullptr;
    RtpTransportFeedbackObserver* transport_feedback_observer = nullptr;
    RtpStreamDataCountersObserver* stream_data_counters_observer = nullptr;
};

struct RtcpConfiguration {
    // True for a audio version of the RTP/RTCP module object false will create
    // a video version.
    bool audio = false;
    bool receiver_only = false;
    
    // Corresponds to extmap-allow-mixed in SDP negotiation.
    bool extmap_allow_mixed = false;

    // SSRCs for media, retransmission(RTX) and FEC.
    uint32_t local_media_ssrc = 0;
    std::optional<uint32_t> rtx_send_ssrc = std::nullopt;
    std::optional<uint32_t> fec_ssrc = std::nullopt;

    int rtcp_report_interval_ms = 0;

    Clock* clock;

    RtcMediaTransport* send_transport;

    // Observers
    RtcpPacketTypeCounterObserver* packet_type_counter_observer = nullptr;
    RtcpIntraFrameObserver* intra_frame_observer = nullptr;
    RtcpLossNotificationObserver* loss_notification_observer = nullptr;
    RtcpBandwidthObserver* bandwidth_observer = nullptr;
    RtcpCnameObserver* cname_observer = nullptr;
    RtcpRttObserver* rtt_observer = nullptr;
    RtcpTransportFeedbackObserver* transport_feedback_observer = nullptr;
    RtcpNackListObserver* nack_list_observer = nullptr;
    RtcpReportBlocksObserver* report_blocks_observer = nullptr;
    RtcpReportBlockProvider* report_block_provider = nullptr;
    RtpSendStatsProvider* rtp_send_stats_provider = nullptr;
};
        
} // namespace naivertc


#endif