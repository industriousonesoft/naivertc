#include "rtc/channels/media_channel.hpp"

#include <plog/Log.h>

namespace naivertc {

MediaChannel::MediaChannel(const std::string mid) 
    : mid_(std::move(mid)),
      task_queue_("MediaChannel."+ mid + ".task.queue") {}

MediaChannel::~MediaChannel() {}

const std::string MediaChannel::mid() const {
    return task_queue_.Sync<std::string>([this](){
        return mid_;
    });
}

bool MediaChannel::is_opened() const {
    return task_queue_.Sync<bool>([this](){
        return is_opened_;
    });
}

void MediaChannel::Open(std::weak_ptr<DtlsSrtpTransport> srtp_transport) {
    task_queue_.Async([this, srtp_transport=std::move(srtp_transport)](){
        if (is_opened_) {
            PLOG_VERBOSE << "MediaChannel: " << mid_ << " did open already.";
            return;
        }
        PLOG_VERBOSE << __FUNCTION__;
        srtp_transport_ = std::move(srtp_transport);
        TriggerOpen();
    });
}

void MediaChannel::Close() {
    task_queue_.Async([this](){
        PLOG_VERBOSE << __FUNCTION__;
        srtp_transport_.reset();
        TriggerClose();
    });
}

void MediaChannel::OnOpened(OpenedCallback callback) {
    task_queue_.Async([this, callback=std::move(callback)](){
        opened_callback_ = callback;
    });
}

void MediaChannel::OnClosed(ClosedCallback callback) {
    task_queue_.Async([this, callback=std::move(callback)](){
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

} // namespace naivertc