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
    
} // namespace naivertc
