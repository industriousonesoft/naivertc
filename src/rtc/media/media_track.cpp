#include "rtc/media/media_track.hpp"

namespace naivertc {
namespace {
MediaChannel::Kind MediaKindFromDescription(const sdp::Media& description) {
    switch (description.kind()) {
    case sdp::MediaEntry::Kind::AUDIO:
        return MediaChannel::Kind::AUDIO;
    case sdp::MediaEntry::Kind::VIDEO:
        return MediaChannel::Kind::VIDEO;
    default:
        return MediaChannel::Kind::UNKNOWN;
    }
}

} // namespace

// Media track
MediaTrack::MediaTrack(sdp::Media description) 
    : MediaChannel(MediaKindFromDescription(description), description.mid()),
      description_(std::move(description)) {}

MediaTrack::~MediaTrack() {}

const sdp::Media* MediaTrack::description() const {
    return task_queue_.Sync<const sdp::Media*>([this](){
        return &description_;
    });
}

void MediaTrack::Reset(sdp::Media description) {
    task_queue_.Async([this, description=std::move(description)](){
        description_ = std::move(description);
        // TODO: Reset other properties.
    });
    
}

} // namespace naivertc