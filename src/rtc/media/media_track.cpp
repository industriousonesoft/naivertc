#include "rtc/media/media_track.hpp"
#include "common/utils_random.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

MediaTrack::Kind ToKind(sdp::MediaEntry::Kind kind) {
    switch(kind) {
    case sdp::MediaEntry::Kind::AUDIO:
        return MediaTrack::Kind::AUDIO;
    case sdp::MediaEntry::Kind::VIDEO:
        return MediaTrack::Kind::VIDEO;
    default:
        RTC_NOTREACHED();
    }
}

} // namespace

// Media track
MediaTrack::MediaTrack(const Configuration& config, TaskQueue* worker_queue) 
    : MediaTrack(SdpBuilder::Build(config), worker_queue) {}

MediaTrack::MediaTrack(sdp::Media description, TaskQueue* worker_queue)
    : MediaChannel(ToKind(description.kind()), description.mid(), worker_queue),
      description_(std::move(description)) {}

MediaTrack::~MediaTrack() {}

sdp::Media MediaTrack::description() const {
    return description_;
}

// Private methods
 void MediaTrack::Open(std::weak_ptr<MediaTransport> transport) {
    MediaChannel::Open(transport);
 }
    
void MediaTrack::Close() {
    MediaChannel::Close();
}

} // namespace naivertc