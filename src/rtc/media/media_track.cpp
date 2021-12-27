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
MediaTrack::MediaTrack(const Configuration& config) 
    : MediaChannel(config.kind(), config.mid()),
      description_(SdpBuilder::Build(config)) {
}

MediaTrack::MediaTrack(sdp::Media description) 
    : MediaChannel(ToKind(description.kind()), description.mid()),
      description_(std::move(description)) {}

MediaTrack::~MediaTrack() {}

sdp::Media MediaTrack::description() const {
    return description_;
}

} // namespace naivertc