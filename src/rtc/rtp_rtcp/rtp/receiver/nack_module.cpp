#include "rtc/rtp_rtcp/rtp/receiver/nack_module.hpp"

#include <plog/Log.h>

namespace naivertc {

// NackModule
NackModule::NackModule(std::shared_ptr<Clock> clock, 
                       int64_t send_nack_delay_ms,
                       TimeDelta update_interval,
                       std::shared_ptr<TaskQueue> task_queue) 
    : impl_(clock, send_nack_delay_ms),
      task_queue_(task_queue) {
    periodic_task_ = RepeatingTask::DelayedStart(clock, task_queue_, update_interval, [this, update_interval]{
        PeriodicUpdate();
        return update_interval;
    });

}

NackModule::~NackModule() {
    periodic_task_->Stop();
}

int NackModule::OnReceivedPacket(uint16_t seq_num, bool is_keyframe, bool is_recovered) {
    return task_queue_->Sync<int>([this, seq_num, is_keyframe, is_recovered](){
        auto ret = impl_.OnReceivedPacket(seq_num, is_keyframe, is_recovered);
        // Are there any nacks that are waiting for this seq_num.
        auto nack_list_to_send = impl_.NackListUpTo(seq_num);
        if (!nack_list_to_send.empty() && nack_list_to_send_callback_) {
            nack_list_to_send_callback_(std::move(nack_list_to_send));
        }
        if (ret.second && request_key_frame_callback_) {
            request_key_frame_callback_();
        }
        return ret.first;
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

void NackModule::OnNackListToSend(NackListToSendCallback callback) {
    task_queue_->Async([this, callback=std::move(callback)](){
        this->nack_list_to_send_callback_ = std::move(callback);
    });
}

void NackModule::OnRequestKeyFrame(RequestKeyFrameCallback callback) {
    task_queue_->Async([this, callback=std::move(callback)](){
        this->request_key_frame_callback_ = std::move(callback);
    });
}

// Private methods
void NackModule::PeriodicUpdate() {
    // Are there any nacks that are waiting to send.
    auto nack_list_to_send = impl_.PeriodicUpdate();
    if (!nack_list_to_send.empty() && nack_list_to_send_callback_) {
        nack_list_to_send_callback_(std::move(nack_list_to_send));
    }
}

} // namespace naivertc
