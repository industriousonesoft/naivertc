#include "rtc/channels/media_channel.hpp"

#include <plog/Log.h>

namespace naivertc {

MediaChannel::MediaChannel(Kind kind, std::string mid, TaskQueue* task_queue) 
    : kind_(kind),
      mid_(std::move(mid)),
      clock_(std::make_unique<RealTimeClock>()),
      task_queue_(task_queue) {}

MediaChannel::~MediaChannel() {}

MediaChannel::Kind MediaChannel::kind() const {
    return task_queue_->Sync<Kind>([this](){
        return kind_;
    });
}

const std::string MediaChannel::mid() const {
    return task_queue_->Sync<std::string>([this](){
        return mid_;
    });
}

bool MediaChannel::is_opened() const {
    return task_queue_->Sync<bool>([this](){
        return is_opened_;
    });
}

void MediaChannel::Open(MediaTransport* transport) {
    task_queue_->Async([this, transport](){
        if (is_opened_) {
            PLOG_VERBOSE << "MediaChannel: " << mid_ << " did open already.";
            return;
        }
        send_transport_ = transport;
        TriggerOpen();
    });
}

void MediaChannel::Close() {
    task_queue_->Async([this](){
        send_transport_ = nullptr;
        TriggerClose();
    });
}

void MediaChannel::OnOpened(OpenedCallback callback) {
    task_queue_->Async([this, callback=std::move(callback)](){
        opened_callback_ = callback;
    });
}

void MediaChannel::OnClosed(ClosedCallback callback) {
    task_queue_->Async([this, callback=std::move(callback)](){
        closed_callback_ = callback;
    });
}

// Private methods
void MediaChannel::TriggerOpen() {
    if (is_opened_) {
        return;
    }
    is_opened_ = true;
    if (opened_callback_) {
        opened_callback_();
    }
}

void MediaChannel::TriggerClose() {
    if (!is_opened_) {
        return;
    }
    is_opened_ = false;
    if (closed_callback_) {
        closed_callback_();
    }
}

// ostream
std::ostream& operator<<(std::ostream& out, MediaChannel::Kind kind) {
    switch(kind) {
    case MediaChannel::Kind::AUDIO:
        out << "audio";
        break;
    case MediaChannel::Kind::VIDEO:
        out << "video";
        break;
    default:
        break;
    }
    return out;
}

} // namespace naivertc