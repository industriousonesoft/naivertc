#include "rtc/media/media_track.hpp"

namespace naivertc {

// Configuration
MediaTrack::Configuration::Configuration::Configuration(const std::string _mid, 
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
MediaTrack::MediaTrack(const sdp::Media description) 
    : MediaChannel(description.mid()),
      description_(std::move(description)) {;
}

MediaTrack::~MediaTrack() {}

sdp::Direction MediaTrack::direction() const {
    return task_queue_.Sync<sdp::Direction>([this](){
        return description_.direction();
    });
}

sdp::Media MediaTrack::description() const {
    return task_queue_.Sync<sdp::Media>([this](){
        return description_;
    });
}

void MediaTrack::UpdateDescription(const sdp::Media description) {
    task_queue_.Async([this, description=std::move(description)](){
        description_ = std::move(description);
    });
    
}

} // namespace naivertc