#include "rtc/rtp_rtcp/rtcp_responser.hpp"

#include <plog/Log.h>

namespace naivertc {

void RtcpResponser::SendNack(std::vector<uint16_t> nack_list,
                          bool buffering_allowed) {
    assert(buffering_allowed == true);
    rtcp_sender_.SendRtcp(GetFeedbackState(), RtcpPacketType::NACK, std::move(nack_list));
}

void RtcpResponser::RequestKeyFrame() {
    rtcp_sender_.SendRtcp(GetFeedbackState(), RtcpPacketType::PLI);
}

RtcpSender::FeedbackState RtcpResponser::GetFeedbackState() {
    RTC_RUN_ON(work_queue_);
    uint32_t received_ntp_secs = 0;
    uint32_t received_ntp_frac = 0;
    feedback_state_.remote_sr = 0;
    auto last_sr_stats = rtcp_receiver_.GetLastSenderReportStats();
    if (last_sr_stats) {
        feedback_state_.remote_sr = ((last_sr_stats->send_ntp_time.seconds() & 0x0000ffff) << 16) +
                                    ((last_sr_stats->send_ntp_time.fractions() & 0xffff0000) >> 16);

        feedback_state_.last_rr_ntp_secs = last_sr_stats->arrival_ntp_time.seconds();
        feedback_state_.last_rr_ntp_frac = last_sr_stats->arrival_ntp_time.fractions();
    }
    return feedback_state_;
}

// Private methods
void RtcpResponser::MaybeSendRtcp() {
    RTC_RUN_ON(work_queue_);
    if (rtcp_sender_.TimeToSendRtcpReport()) {
        rtcp_sender_.SendRtcp(GetFeedbackState(), RtcpPacketType::REPORT);
    }
}

void RtcpResponser::ScheduleRtcpSendEvaluation(TimeDelta delay) {
    if (delay.IsZero()) {
        work_queue_->Post([this](){
            this->MaybeSendRtcp();
        });
    } else {
        Timestamp execution_time = clock_->CurrentTime() + delay;
        work_queue_->PostDelayed(delay, [this, execution_time](){
            this->MaybeSendRtcpAtOrAfterTimestamp(execution_time);
        });
    }
}

void RtcpResponser::MaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time) {
    Timestamp now = clock_->CurrentTime();
    if (now >= execution_time) {
        MaybeSendRtcp();
        return;
    }

    PLOG_WARNING << "TaskQueueBug: Task queue scheduled delayed call too early.";

    TimeDelta delay = execution_time - now;
    work_queue_->PostDelayed(delay, [this, execution_time](){
        this->MaybeSendRtcpAtOrAfterTimestamp(execution_time);
    });
}

} // namespace naivertc