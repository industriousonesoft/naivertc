#include "rtc/channels/media_channel.hpp"

#include <plog/Log.h>

namespace naivertc {

MediaChannel::MediaChannel(Kind kind, 
                           std::string mid)
    : kind_(kind),
      mid_(std::move(mid)),
      signaling_queue_(TaskQueueImpl::Current()) {
    assert(signaling_queue_ != nullptr);
}

MediaChannel::~MediaChannel() {}

MediaChannel::Kind MediaChannel::kind() const {
    return kind_;
}

const std::string MediaChannel::mid() const {
    return mid_;
}

bool MediaChannel::is_opened() const {
    RTC_RUN_ON(signaling_queue_);
    return is_opened_;
}

void MediaChannel::OnOpened(OpenedCallback callback) {
    signaling_queue_->Post([this, callback=std::move(callback)](){
        opened_callback_ = callback;
        if (is_opened_ && opened_callback_) {
            opened_callback_();
        }
    });
}

void MediaChannel::OnClosed(ClosedCallback callback) {
    signaling_queue_->Post([this, callback=std::move(callback)](){
        closed_callback_ = callback;
    });
}

void MediaChannel::Open() {
    RTC_RUN_ON(signaling_queue_);
    TriggerOpen();
}

void MediaChannel::Close() {
    RTC_RUN_ON(signaling_queue_);
    TriggerClose();
    PLOG_VERBOSE << "Media channel closed.";
}

// Private methods
void MediaChannel::TriggerOpen() {
    RTC_RUN_ON(signaling_queue_);
    if (is_opened_) {
        return;
    }
    is_opened_ = true;
    if (opened_callback_) {
        opened_callback_();
    }
}

void MediaChannel::TriggerClose() {
    RTC_RUN_ON(signaling_queue_);
    if (!is_opened_) {
        return;
    }
    is_opened_ = false;
    if (closed_callback_) {
        closed_callback_();
    }
}

} // namespace naivertc