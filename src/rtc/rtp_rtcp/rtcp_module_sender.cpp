#include "rtc/rtp_rtcp/rtcp_module.hpp"

#include <plog/Log.h>

namespace naivertc {

void RtcpModule::SendNack(std::vector<uint16_t> nack_list,
                          bool buffering_allowed) {
    assert(buffering_allowed == true);
    rtcp_sender_.SendRtcp(GetFeedbackState(), RtcpPacketType::NACK, std::move(nack_list));
}

void RtcpModule::RequestKeyFrame() {
    rtcp_sender_.SendRtcp(GetFeedbackState(), RtcpPacketType::PLI);
}

// Private methods
const RtcpSender::FeedbackState& RtcpModule::GetFeedbackState() {
    RTC_RUN_ON(work_queue_);
    uint32_t received_ntp_secs = 0;
    uint32_t received_ntp_frac = 0;
    feedback_state_.remote_sr = 0;
    if (rtcp_receiver_.NTP(&received_ntp_secs, &received_ntp_frac,
                            /*rtcp_arrival_time_secs=*/&feedback_state_.last_rr_ntp_secs,
                            /*rtcp_arrival_time_frac=*/&feedback_state_.last_rr_ntp_frac,
                            /*rtcp_timestamp=*/nullptr,
                            /*remote_sender_packet_count=*/nullptr,
                            /*remote_sender_octet_count=*/nullptr,
                            /*remote_sender_reports_count=*/nullptr)) {
        feedback_state_.remote_sr = ((received_ntp_secs & 0x0000ffff) << 16) +
                                    ((received_ntp_frac & 0xffff0000) >> 16);
    }
    return feedback_state_;
}

void RtcpModule::MaybeSendRtcp() {
    RTC_RUN_ON(work_queue_);
    if (rtcp_sender_.TimeToSendRtcpReport()) {
        rtcp_sender_.SendRtcp(GetFeedbackState(), RtcpPacketType::REPORT);
    }
}

void RtcpModule::ScheduleRtcpSendEvaluation(TimeDelta delay) {
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

void RtcpModule::MaybeSendRtcpAtOrAfterTimestamp(Timestamp execution_time) {
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