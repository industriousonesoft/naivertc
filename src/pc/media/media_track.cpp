#include "pc/media/media_track.hpp"

namespace naivertc {

MediaTrack::MediaTrack(const sdp::Media& description) 
    : description_(std::move(description)) {;
}

MediaTrack::~MediaTrack() {}

std::string MediaTrack::mid() const {
    return description_.mid();
}

sdp::Direction MediaTrack::direction() const {
    return description_.direction();
}

sdp::Media MediaTrack::description() const {
    return description_;
}

void MediaTrack::UpdateDescription(const sdp::Media& description) {
    description_ = std::move(description);
}

} // namespace naivertc