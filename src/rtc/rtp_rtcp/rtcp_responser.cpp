#include "rtc/rtp_rtcp/rtcp_responser.hpp"

namespace naivertc {
namespace {

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
    : clock_(config.clock),
      rtcp_sender_(RtcpConfigurationFromRtpRtcpConfiguration(config, this)),
      rtcp_receiver_(config),
      work_queue_(TaskQueueImpl::Current()),
      rtt_ms_(0) {

    // TODO: RTT PeriodicUpdate
}

RtcpResponser::~RtcpResponser() {}

void RtcpResponser::set_rtt_ms(int64_t rtt_ms) {
    RTC_RUN_ON(&sequence_checker_);
    rtt_ms_ = rtt_ms;
}

int64_t RtcpResponser::rtt_ms() const {
    RTC_RUN_ON(&sequence_checker_);
    return rtt_ms_;
}

void RtcpResponser::set_remote_ssrc(uint32_t remote_ssrc) {
    RTC_RUN_ON(&sequence_checker_);
    rtcp_sender_.set_remote_ssrc(remote_ssrc);
    rtcp_receiver_.set_remote_ssrc(remote_ssrc);
}
    
} // namespace naivertc
