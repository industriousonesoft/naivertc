#include "rtc/media/media_track.hpp"

namespace naivertc {

// Configuration
MediaTrack::Configuration::Configuration::Configuration(std::string mid, 
                                                        Kind kind, 
                                                        Codec codec, 
                                                        std::vector<int> payload_types, 
                                                        uint32_t ssrc, 
                                                        std::optional<std::string> cname, 
                                                        std::optional<std::string> msid,
                                                        std::optional<std::string> track_id) 
    : mid(std::move(mid)),
      kind(kind),
      codec(codec),
      payload_types(std::move(payload_types)),
      ssrc(ssrc),
      cname(std::move(cname)),
      msid(std::move(msid)),
      track_id(std::move(track_id)) {}

// Media track
MediaTrack::MediaTrack(sdp::Media description) 
    : MediaChannel(description.mid()),
      description_(std::move(description)) {}

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

void MediaTrack::UpdateDescription(sdp::Media description) {
    task_queue_.Async([this, description=std::move(description)](){
        description_ = std::move(description);
    });
    
}

} // namespace naivertc