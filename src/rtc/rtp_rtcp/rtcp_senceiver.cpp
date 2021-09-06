#include "rtc/rtp_rtcp/rtcp_senceiver.hpp"

namespace naivertc {

RtcpSenceriver::RtcpSenceriver(const RtcpConfiguration& config,
                               std::shared_ptr<TaskQueue> task_queue)
    : clock_(config.clock),
      task_queue_(task_queue),
      rtcp_sender_(config, task_queue_),
      rtcp_receiver_(config, this, task_queue_),
      work_queue_("com.RtcpSenceriver.work.queue") {

    rtcp_sender_.OnNextSendEvaluationTimeScheduled(std::bind(&RtcpSenceriver::ScheduleRtcpSendEvaluation, this, std::placeholders::_1));
}

RtcpSenceriver::~RtcpSenceriver() {

}
    
} // namespace naivertc
