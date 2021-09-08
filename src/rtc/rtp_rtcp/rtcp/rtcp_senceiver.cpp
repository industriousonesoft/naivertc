#include "rtc/rtp_rtcp/rtcp/rtcp_senceiver.hpp"
#include "rtc/base/units/bit_rate.hpp"

namespace naivertc {

RtcpSenceiver::RtcpSenceiver(const RtcpConfiguration& config,
                               std::shared_ptr<TaskQueue> task_queue)
    : clock_(config.clock),
      task_queue_(task_queue),
      rtcp_sender_(config, task_queue_),
      rtcp_receiver_(config, this, task_queue_),
      work_queue_("com.RtcpSenceiver.work.queue") {

    rtcp_sender_.OnNextSendEvaluationTimeScheduled(std::bind(&RtcpSenceiver::ScheduleRtcpSendEvaluation, this, std::placeholders::_1));

    // TODO: RTT PeriodicUpdate
}

RtcpSenceiver::~RtcpSenceiver() {

}

// Private methods
// RtpSentStatisticsObserver
void RtcpSenceiver::RtpSentCountersUpdated(const RtpSentCounters& rtp_stats, const RtpSentCounters& rtx_stats) {
    work_queue_.Async([this, &rtp_stats, &rtx_stats](){
        feedback_state_.packets_sent = rtp_stats.transmitted.packets + rtx_stats.transmitted.packets;
        feedback_state_.media_bytes_sent = rtp_stats.transmitted.payload_bytes + rtx_stats.transmitted.payload_bytes;
    });
}

void RtcpSenceiver::RtpSentBitRateUpdated(const BitRate bit_rate) {
    work_queue_.Async([this, bit_rate=std::move(bit_rate)](){
        feedback_state_.send_bitrate = bit_rate.bps<uint32_t>();
    });
}
    
} // namespace naivertc
