#include "rtc/rtp_rtcp/rtp_rtcp_impl.hpp"

#include <plog/Log.h>

namespace naivertc {

RtcpSender::Configuration AddRtcpSendEvaluationCallback(
        RtcpSender::Configuration config, 
        std::function<void(TimeDelta)> send_evaluation_callback) {
    config.schedule_next_rtcp_send_evaluation_function =
      std::move(send_evaluation_callback);
  return config;
}

RtpRtcpImpl::RtpRtcpImpl(const RtpRtcpInterface::Configuration& config, std::shared_ptr<TaskQueue> task_queue) 
    : task_queue_(task_queue ? task_queue : std::make_shared<TaskQueue>("RtpRtcpImpl.task.queue")),
    clock_(config.clock),
    rtcp_sender_(AddRtcpSendEvaluationCallback(
          RtcpSender::Configuration::FromRtpRtcpConfiguration(config),
          [this](TimeDelta duration) {
              ScheduleRtcpSendEvaluation(duration);
          }), config.clock, task_queue_),
    rtcp_receiver_(RtcpReceiver::Configuration::FromRtpRtcpConfiguration(config), this, config.clock, task_queue_) {

}

RtpRtcpImpl::~RtpRtcpImpl() {
    
}


uint32_t RtpRtcpImpl::StartTimestamp() const {
    // TODO: Implement
    return 0;
}

void RtpRtcpImpl::SetStartTimestamp(uint32_t timestamp) {
    // TODO: Implement
}

RtcpSender::FeedbackState RtpRtcpImpl::GetFeedbackState() {
    RtcpSender::FeedbackState state;
    // TODO: Init feedback state
    return state;
}

// Private methods
void RtpRtcpImpl::ScheduleRtcpSendEvaluation(TimeDelta duration) {
    if (duration.IsZero()) {
        task_queue_->Async([this](){
            this->MaybeSendRtcp();
        });
    }else {
        Timestamp execution_time = clock_->CurrentTime() + duration;
        ScheduleMaybeSendRtcpAtOrAfterTimestamp(execution_time, duration);
    }
}

void RtpRtcpImpl::ScheduleMaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time, TimeDelta duration) {
    task_queue_->AsyncAfter(duration.seconds(), [this, execution_time](){
        this->MaybeSendRtcpAtOrAfterTimestamp(execution_time);
    });
}

void RtpRtcpImpl::MaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time) {
    Timestamp now = clock_->CurrentTime();
    if (now >= execution_time) {
        MaybeSendRtcp();
        return;
    }

    PLOG_WARNING << "TaskQueueBug: Task queue scheduled delayed call too early.";

    ScheduleMaybeSendRtcpAtOrAfterTimestamp(execution_time, execution_time - now);
}

void RtpRtcpImpl::MaybeSendRtcp() {
    if (rtcp_sender_.TimeToSendRtcpReport()) {
        rtcp_sender_.SendRtcp(GetFeedbackState(), RtcpPacketType::REPORT);
    }
}

} // namespace naivertc