#include "rtc/rtp_rtcp/rtcp_module.hpp"

namespace naivertc {

RtcpModule::RtcpModule(const RtcpConfiguration& config,
                       std::shared_ptr<TaskQueue> task_queue)
    : clock_(config.clock),
      task_queue_(task_queue),
      rtcp_sender_(config, task_queue_),
      rtcp_receiver_(config, this, task_queue_),
      work_queue_("com.RtcpModule.work.queue"),
      rtt_ms_(0) {

    rtcp_sender_.OnNextSendEvaluationTimeScheduled(std::bind(&RtcpModule::ScheduleRtcpSendEvaluation, this, std::placeholders::_1));

    // TODO: RTT PeriodicUpdate
}

RtcpModule::~RtcpModule() {}

void RtcpModule::set_rtt_ms(int64_t rtt_ms) {
    task_queue_->Async([this, rtt_ms](){
        rtt_ms_ = rtt_ms;
    });
}

int64_t RtcpModule::rtt_ms() const {
    return task_queue_->Sync<int64_t>([this](){
        return rtt_ms_;
    });
}

// Private methods
// RtpSentStatisticsObserver
void RtcpModule::RtpSentCountersUpdated(const RtpSentCounters& rtp_stats, const RtpSentCounters& rtx_stats) {
    work_queue_.Async([this, &rtp_stats, &rtx_stats](){
        feedback_state_.packets_sent = rtp_stats.transmitted.packets + rtx_stats.transmitted.packets;
        feedback_state_.media_bytes_sent = rtp_stats.transmitted.payload_bytes + rtx_stats.transmitted.payload_bytes;
    });
}

void RtcpModule::RtpSentBitRateUpdated(const DataRate bit_rate) {
    work_queue_.Async([this, bit_rate=std::move(bit_rate)](){
        feedback_state_.send_bitrate = bit_rate.bps<uint32_t>();
    });
}
    
} // namespace naivertc
