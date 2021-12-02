#include "rtc/rtp_rtcp/rtp/receiver/nack_module.hpp"

#include <plog/Log.h>

namespace naivertc {

// NackModule
NackModule::NackModule(Clock* clock,
                       NackSender* nack_sender,
                       KeyFrameRequestSender* key_frame_request_sender,
                       int64_t send_nack_delay_ms,
                       TimeDelta update_interval) 
    : impl_(clock, send_nack_delay_ms),
      nack_sender_(std::move(nack_sender)),
      key_frame_request_sender_(std::move(key_frame_request_sender)) {
    RTC_RUN_ON(&sequence_checker_);
    periodic_task_ = RepeatingTask::DelayedStart(clock, TaskQueueImpl::Current(), update_interval, [this, update_interval]{
        PeriodicUpdate();
        return update_interval;
    });
}

NackModule::~NackModule() {
    RTC_RUN_ON(&sequence_checker_);
    periodic_task_->Stop();
}

size_t NackModule::InsertPacket(uint16_t seq_num, bool is_keyframe, bool is_recovered) {
    RTC_RUN_ON(&sequence_checker_);
    auto ret = impl_.InsertPacket(seq_num, is_keyframe, is_recovered);
    if (ret.keyframe_requested) {
        if (key_frame_request_sender_) {
            key_frame_request_sender_->RequestKeyFrame();
        }
    }
    if (!ret.nack_list_to_send.empty()) {
        if (nack_sender_) {
            // The list of NACK is triggered externally,
            // the caller can send them with other feedback messages.
            nack_sender_->SendNack(std::move(ret.nack_list_to_send), true /* buffering_allowed */);
        }
    }
    return ret.nacks_sent_for_seq_num;
}

void NackModule::ClearUpTo(uint16_t seq_num) {
    RTC_RUN_ON(&sequence_checker_);
    impl_.ClearUpTo(seq_num);
}

void NackModule::UpdateRtt(int64_t rtt_ms) {
    RTC_RUN_ON(&sequence_checker_);
    impl_.UpdateRtt(rtt_ms);
}

// Private methods
void NackModule::PeriodicUpdate() {
    RTC_RUN_ON(&sequence_checker_);
    // Are there any nacks that are waiting to send.
    auto nack_list_to_send = impl_.NackListOnRttPassed();
    if (!nack_list_to_send.empty()) {
        if (nack_sender_) {
            // The list of NACK is triggered periodically,
            // there is no caller who can send them with other
            // feedback messages.
            nack_sender_->SendNack(std::move(nack_list_to_send), false /* buffering_allowed */);
        }
    }
}

} // namespace naivertc
