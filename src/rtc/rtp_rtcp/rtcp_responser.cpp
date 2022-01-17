#include "rtc/rtp_rtcp/rtcp_responser.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

const int64_t kDefaultExpectedRetransmissionTimeMs = 125;

RtcpSender::Configuration RtcpConfigurationFromRtpRtcpConfiguration(const RtcpConfiguration& config, 
                                                                    RtcpReceiveFeedbackProvider* rtcp_receive_feedback_provider) {
    RtcpSender::Configuration rtcp_sender_config;
    rtcp_sender_config.audio = config.audio;
    rtcp_sender_config.local_media_ssrc = config.local_media_ssrc;
    rtcp_sender_config.clock = config.clock;
    rtcp_sender_config.rtcp_report_interval_ms = config.rtcp_report_interval_ms;
    rtcp_sender_config.send_transport = config.send_transport;
    rtcp_sender_config.packet_type_counter_observer = config.packet_type_counter_observer;
    rtcp_sender_config.report_block_provider = config.report_block_provider;
    rtcp_sender_config.rtp_send_feedback_provider = config.rtp_send_feedback_provider;
    rtcp_sender_config.rtcp_receive_feedback_provider = rtcp_receive_feedback_provider;
    return rtcp_sender_config;
}

} // namespace

RtcpResponser::RtcpResponser(const RtcpConfiguration& config)
    : rtcp_sender_(RtcpConfigurationFromRtpRtcpConfiguration(config, this)),
      rtcp_receiver_(config) {}

RtcpResponser::~RtcpResponser() {}

void RtcpResponser::set_remote_ssrc(uint32_t remote_ssrc) {
    RTC_RUN_ON(&sequence_checker_);
    rtcp_sender_.set_remote_ssrc(remote_ssrc);
    rtcp_receiver_.set_remote_ssrc(remote_ssrc);
}

TimeDelta RtcpResponser::rtt() const {
    return rtcp_receiver_.rtt();
}

void RtcpResponser::IncomingPacket(const uint8_t* packet, size_t packet_size) {
    RTC_RUN_ON(&sequence_checker_);
    IncomingPacket(CopyOnWriteBuffer(packet, packet_size));
}
    
void RtcpResponser::IncomingPacket(CopyOnWriteBuffer rtcp_packet) {
    RTC_RUN_ON(&sequence_checker_);
    rtcp_receiver_.IncomingPacket(std::move(rtcp_packet));
}

void RtcpResponser::SendNack(const std::vector<uint16_t>& nack_list,
                             bool buffering_allowed) {
    RTC_RUN_ON(&sequence_checker_);
    assert(buffering_allowed == true);
    rtcp_sender_.SendRtcp(RtcpPacketType::NACK, nack_list.data(), nack_list.size());
}

void RtcpResponser::RequestKeyFrame() {
    RTC_RUN_ON(&sequence_checker_);
    rtcp_sender_.SendRtcp(RtcpPacketType::PLI);
}

std::optional<RttStats> RtcpResponser::GetRttStats(uint32_t ssrc) const {
    RTC_RUN_ON(&sequence_checker_);
    return rtcp_receiver_.GetRttStats(ssrc);
}

int64_t RtcpResponser::ExpectedRestransmissionTimeMs() const {
    RTC_RUN_ON(&sequence_checker_);
    auto expected_retransmission_time = rtcp_receiver_.rtt();
    if (expected_retransmission_time.IsFinite()) {
        return expected_retransmission_time.ms();
    }

    // If no RTT available yet, so try to retrieve avg_rtt_ms directly
    // from RTCP receiver.
    auto rtt_stats = rtcp_receiver_.GetRttStats(rtcp_receiver_.remote_ssrc());
    if (rtt_stats) {
        return rtt_stats->avg_rtt().ms();
    }
    return kDefaultExpectedRetransmissionTimeMs;
}

RtcpReceiveFeedback RtcpResponser::GetReceiveFeedback() {
    RTC_RUN_ON(&sequence_checker_);
    RtcpReceiveFeedback receive_feedback;
    receive_feedback.last_sr_stats = rtcp_receiver_.GetLastSenderReportStats();;
    return receive_feedback;
}

    
} // namespace naivertc
