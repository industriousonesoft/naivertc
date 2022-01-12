#include "rtc/rtp_rtcp/rtcp_responser.hpp"

namespace naivertc {

RtcpResponser::RtcpResponser(const RtcpConfiguration& config)
    : clock_(config.clock),
      rtcp_sender_(config),
      rtcp_receiver_(config, this),
      work_queue_(TaskQueueImpl::Current()),
      rtt_ms_(0) {

    rtcp_sender_.OnNextSendEvaluationTimeScheduled(std::bind(&RtcpResponser::ScheduleRtcpSendEvaluation, this, std::placeholders::_1));

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

// Private methods
// RtpSentStatisticsObserver
void RtcpResponser::RtpSentCountersUpdated(const RtpSentCounters& rtp_stats, const RtpSentCounters& rtx_stats) {
    work_queue_->Post([this, &rtp_stats, &rtx_stats](){
        feedback_state_.packets_sent = rtp_stats.transmitted.num_packets + rtx_stats.transmitted.num_packets;
        feedback_state_.media_bytes_sent = rtp_stats.transmitted.payload_bytes + rtx_stats.transmitted.payload_bytes;
    });
}

void RtcpResponser::RtpSentBitRateUpdated(const DataRate bit_rate) {
    work_queue_->Post([this, bit_rate=std::move(bit_rate)](){
        feedback_state_.send_bitrate = bit_rate.bps<uint32_t>();
    });
}
    
} // namespace naivertc
