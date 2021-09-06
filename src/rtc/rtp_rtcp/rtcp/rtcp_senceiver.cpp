#include "rtc/rtp_rtcp/rtcp/rtcp_senceiver.hpp"

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
// RtpSender oberver
void RtcpSenceiver::RtpSentCountersUpdated(const RtpSentCounters& rtp_stats, const RtpSentCounters& rtx_stats) {
    work_queue_.Async([&](){
        feedback_state_.packets_sent = rtp_stats.transmitted.packets + rtx_stats.transmitted.packets;
        feedback_state_.media_bytes_sent = rtp_stats.transmitted.payload_bytes + rtx_stats.transmitted.payload_bytes;
        // TODO: Calculate send bitrate
        // feedback_state_.send_bitrate
    });
}
    
} // namespace naivertc
