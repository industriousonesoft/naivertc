#include "rtc/call/media_send_stream.hpp"

#include <plog/Log.h>

namespace naivertc {
// RtcpSender
// Private methods
RtcpSender::FeedbackState MediaSendStream::GetFeedbackState() {
    RtcpSender::FeedbackState state;
    // TODO: Create Feedback state
    return state;
}

void MediaSendStream::MaybeSendRtcp() {
    if (rtcp_sender_->TimeToSendRtcpReport()) {
        rtcp_sender_->SendRtcp(GetFeedbackState(), RtcpPacketType::REPORT);
    }
}

void MediaSendStream::ScheduleRtcpSendEvaluation(TimeDelta delay) {
    if (delay.IsZero()) {
        rtcp_task_queue_.Async([this](){
            this->MaybeSendRtcp();
        });
    }else {
        Timestamp execution_time = clock_->CurrentTime() + delay;
        rtcp_task_queue_.AsyncAfter(delay.seconds(), [this, execution_time](){
            this->MaybeSendRtcpAtOrAfterTimestamp(execution_time);
        });
    }
}

void MediaSendStream::MaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time) {
    Timestamp now = clock_->CurrentTime();
    if (now >= execution_time) {
        MaybeSendRtcp();
        return;
    }

    PLOG_WARNING << "TaskQueueBug: Task queue scheduled delayed call too early.";

    TimeDelta delay = execution_time - now;
    rtcp_task_queue_.AsyncAfter(delay.seconds(), [this, execution_time](){
        this->MaybeSendRtcpAtOrAfterTimestamp(execution_time);
    });
}

// RtcpReceiver 
// Observer methods
void MediaSendStream::SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set) {

}

void MediaSendStream::OnRequestSendReport() {

}

void MediaSendStream::OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers) {

}

void MediaSendStream::OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks) {

}


} // namespace naivertc