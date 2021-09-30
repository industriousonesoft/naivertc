#include "rtc/rtp_rtcp/rtcp_module.hpp"
#include "rtc/base/units/bit_rate.hpp"

namespace naivertc {

RtcpModule::RtcpModule(const RtcpConfiguration& config,
                       std::shared_ptr<TaskQueue> task_queue)
    : clock_(config.clock),
      task_queue_(task_queue),
      rtcp_sender_(config, task_queue_),
      rtcp_receiver_(config, this, task_queue_),
      work_queue_("com.RtcpModule.work.queue") {

    rtcp_sender_.OnNextSendEvaluationTimeScheduled(std::bind(&RtcpModule::ScheduleRtcpSendEvaluation, this, std::placeholders::_1));

    // TODO: RTT PeriodicUpdate
}

RtcpModule::~RtcpModule() {

}

// Private methods
// RtpSentStatisticsObserver
void RtcpModule::RtpSentCountersUpdated(const RtpSentCounters& rtp_stats, const RtpSentCounters& rtx_stats) {
    work_queue_.Async([this, &rtp_stats, &rtx_stats](){
        feedback_state_.packets_sent = rtp_stats.transmitted.packets + rtx_stats.transmitted.packets;
        feedback_state_.media_bytes_sent = rtp_stats.transmitted.payload_bytes + rtx_stats.transmitted.payload_bytes;
    });
}

void RtcpModule::RtpSentBitRateUpdated(const BitRate bit_rate) {
    work_queue_.Async([this, bit_rate=std::move(bit_rate)](){
        feedback_state_.send_bitrate = bit_rate.bps<uint32_t>();
    });
}
    
} // namespace naivertc
