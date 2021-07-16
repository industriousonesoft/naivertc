#include "rtc/media/media_track.hpp"

namespace naivertc {

// Config
MediaTrack::Config::Config::Config(std::string _mid, 
                                    Kind _kind, 
                                    Codec _codec, 
                                    const std::vector<int> _payload_types, 
                                    uint32_t _ssrc, 
                                    std::optional<std::string> _cname, 
                                    std::optional<std::string> _msid,
                                    std::optional<std::string> _track_id) 
    : mid(std::move(_mid)),
    kind(_kind),
    codec(_codec),
    payload_types(std::move(_payload_types)),
    ssrc(_ssrc),
    cname(std::move(_cname)),
    msid(std::move(_msid)),
    track_id(std::move(_track_id)) {}

// Media track
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