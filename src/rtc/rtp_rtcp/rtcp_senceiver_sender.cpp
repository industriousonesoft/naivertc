#include "rtc/rtp_rtcp/rtcp_senceiver.hpp"

#include <plog/Log.h>

namespace naivertc {

RtcpSender::FeedbackState RtcpSenceriver::GetFeedbackState() {
    RtcpSender::FeedbackState state;
    // TODO: Create Feedback state
    return state;
}

void RtcpSenceriver::MaybeSendRtcp() {
    if (rtcp_sender_.TimeToSendRtcpReport()) {
        rtcp_sender_.SendRtcp(GetFeedbackState(), RtcpPacketType::REPORT);
    }
}

void RtcpSenceriver::ScheduleRtcpSendEvaluation(TimeDelta delay) {
    if (delay.IsZero()) {
        work_queue_.Async([this](){
            this->MaybeSendRtcp();
        });
    }else {
        Timestamp execution_time = clock_->CurrentTime() + delay;
        work_queue_.AsyncAfter(delay.seconds(), [this, execution_time](){
            this->MaybeSendRtcpAtOrAfterTimestamp(execution_time);
        });
    }
}

void RtcpSenceriver::MaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time) {
    Timestamp now = clock_->CurrentTime();
    if (now >= execution_time) {
        MaybeSendRtcp();
        return;
    }

    PLOG_WARNING << "TaskQueueBug: Task queue scheduled delayed call too early.";

    TimeDelta delay = execution_time - now;
    work_queue_.AsyncAfter(delay.seconds(), [this, execution_time](){
        this->MaybeSendRtcpAtOrAfterTimestamp(execution_time);
    });
}

} // namespace naivertc