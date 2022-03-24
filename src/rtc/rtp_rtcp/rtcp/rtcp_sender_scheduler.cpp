#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"

#include <plog/Log.h>

#define ENABLE_UNIT_TESTS 0

namespace naivertc {

void RtcpSender::MaybeSendRtcp() {
    RTC_RUN_ON(work_queue_);
    if (TimeToSendRtcpReport()) {
        SendRtcp(RtcpPacketType::RTCP_REPORT);
    }
}

void RtcpSender::ScheduleForNextRtcpSend(TimeDelta delay) {
    RTC_RUN_ON(&sequence_checker_);
    next_time_to_send_rtcp_ = clock_->CurrentTime() + delay;
#if ENABLE_UNIT_TESTS
    // NOTE: The unit tests not supports the task queue so far.
    return;
#endif
    if (delay.IsZero()) {
        work_queue_->Post(ToQueuedTask(task_safety_, [this](){
            this->MaybeSendRtcp();
        }));
    } else {
        Timestamp execution_time = clock_->CurrentTime() + delay;
        work_queue_->PostDelayed(delay, ToQueuedTask(task_safety_, [this, execution_time](){
            this->MaybeSendRtcpAtOrAfterTimestamp(execution_time);
        }));
    }
}

void RtcpSender::MaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time) {
    RTC_RUN_ON(work_queue_);
    Timestamp now = clock_->CurrentTime();
    if (now >= execution_time) {
        MaybeSendRtcp();
        return;
    }

    PLOG_WARNING << "TaskQueueBug: Task queue scheduled delayed call too early.";

    TimeDelta delay = execution_time - now;
    work_queue_->PostDelayed(delay, ToQueuedTask(task_safety_, [this, execution_time](){
        this->MaybeSendRtcpAtOrAfterTimestamp(execution_time);
    }));
}
    
} // namespace naivertc
