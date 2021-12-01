#include "rtc/rtp_rtcp/rtp/receiver/nack_module.hpp"

#include <plog/Log.h>

namespace naivertc {

// NackModule
NackModule::NackModule(std::shared_ptr<Clock> clock,
                       std::shared_ptr<TaskQueue> task_queue,
                       std::weak_ptr<NackSender> nack_sender,
                       std::weak_ptr<KeyFrameRequestSender> key_frame_request_sender,
                       int64_t send_nack_delay_ms,
                       TimeDelta update_interval) 
    : impl_(clock, send_nack_delay_ms),
      task_queue_(std::move(task_queue)),
      nack_sender_(std::move(nack_sender)),
      key_frame_request_sender_(std::move(key_frame_request_sender)) {
    assert(task_queue != nullptr && "Task queue is not supposed to be null.");
    periodic_task_ = RepeatingTask::DelayedStart(clock.get(), task_queue_->Get(), update_interval, [this, update_interval]{
        PeriodicUpdate();
        return update_interval;
    });
}

NackModule::~NackModule() {
    periodic_task_->Stop();
}

size_t NackModule::InsertPacket(uint16_t seq_num, bool is_keyframe, bool is_recovered) {
    return task_queue_->Sync<size_t>([this, seq_num, is_keyframe, is_recovered](){
        auto ret = impl_.InsertPacket(seq_num, is_keyframe, is_recovered);
        if (ret.keyframe_requested) {
            if (auto sender = key_frame_request_sender_.lock()) {
                sender->RequestKeyFrame();
            }
        }
        if (!ret.nack_list_to_send.empty()) {
            if (auto sender = nack_sender_.lock()) {
                // The list of NACK is triggered externally,
                // the caller can send them with other feedback messages.
                sender->SendNack(std::move(ret.nack_list_to_send), true /* buffering_allowed */);
            }
        }
        return ret.nacks_sent_for_seq_num;
    });
}

void NackModule::ClearUpTo(uint16_t seq_num) {
    task_queue_->Async([this, seq_num](){
        impl_.ClearUpTo(seq_num);
    });
}

void NackModule::UpdateRtt(int64_t rtt_ms) {
    task_queue_->Async([this, rtt_ms](){
        impl_.UpdateRtt(rtt_ms);
    });
}

// Private methods
void NackModule::PeriodicUpdate() {
    // Are there any nacks that are waiting to send.
    auto nack_list_to_send = impl_.NackListOnRttPassed();
    if (!nack_list_to_send.empty()) {
        if (auto sender = nack_sender_.lock()) {
            // The list of NACK is triggered periodically,
            // there is no caller who can send them with other
            // feedback messages.
            sender->SendNack(std::move(nack_list_to_send), false /* buffering_allowed */);
        }
    }
}

} // namespace naivertc
