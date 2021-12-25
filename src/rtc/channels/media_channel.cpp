#include "rtc/channels/media_channel.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

std::string ToString(MediaChannel::Kind kind) {
    return kind == MediaChannel::Kind::AUDIO ? "audio" : "video";
}
    
} // namespace

MediaChannel::MediaChannel(Kind kind, std::string mid) 
    : kind_(kind),
      mid_(std::move(mid)),
      clock_(std::make_unique<RealTimeClock>()),
      signaling_queue_(std::make_unique<TaskQueue>(ToString(kind_) + ".mediachannel." + mid + "signaling.queue")) {}

MediaChannel::~MediaChannel() {}

MediaChannel::Kind MediaChannel::kind() const {
    return signaling_queue_->Sync<Kind>([this](){
        return kind_;
    });
}

const std::string MediaChannel::mid() const {
    return signaling_queue_->Sync<std::string>([this](){
        return mid_;
    });
}

bool MediaChannel::is_opened() const {
    return signaling_queue_->Sync<bool>([this](){
        return is_opened_;
    });
}

void MediaChannel::Open(MediaTransport* transport) {
    signaling_queue_->Async([this, transport](){
        if (is_opened_) {
            PLOG_VERBOSE << "MediaChannel: " << mid_ << " did open already.";
            return;
        }
        send_transport_ = transport;
        TriggerOpen();
    });
}

void MediaChannel::Close() {
    signaling_queue_->Async([this](){
        send_transport_ = nullptr;
        TriggerClose();
    });
}

void MediaChannel::OnOpened(OpenedCallback callback) {
    signaling_queue_->Async([this, callback=std::move(callback)](){
        opened_callback_ = callback;
    });
}

void MediaChannel::OnClosed(ClosedCallback callback) {
    signaling_queue_->Async([this, callback=std::move(callback)](){
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