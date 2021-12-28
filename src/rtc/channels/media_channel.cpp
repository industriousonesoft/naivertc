#include "rtc/channels/media_channel.hpp"

#include <plog/Log.h>

namespace naivertc {


MediaChannel::MediaChannel(std::string mid, TaskQueue* worker_queue) 
    : mid_(std::move(mid)),
      worker_queue_(worker_queue) {}

MediaChannel::~MediaChannel() {}

const std::string MediaChannel::mid() const {
    return mid_;
}

void MediaChannel::Open(std::weak_ptr<MediaTransport> transport) {
    worker_queue_->Async([this, transport=std::move(transport)](){
        send_transport_ = std::move(transport);
    });
}

void MediaChannel::Close() {
    worker_queue_->Async([this](){
        send_transport_.reset();
    });
}

void MediaChannel::SetLocalMedia(sdp::Media media, sdp::Type type) {}

void MediaChannel::SetRemoteMedia(sdp::Media media, sdp::Type type) {}

// Private methods

} // namespace naivertc