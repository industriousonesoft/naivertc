#include "rtc/media/media_track.hpp"

namespace naivertc {

// Configuration
MediaTrack::Configuration::Configuration::Configuration(std::string mid, 
                                                        Kind kind, 
                                                        Codec codec) 
    : mid(std::move(mid)),
      kind(kind),
      codec(codec) {}

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

void MediaTrack::set_description(sdp::Media description) {
    task_queue_.Async([this, description=std::move(description)](){
        description_ = std::move(description);
    });
    
}

std::ostream& operator<<(std::ostream& out, MediaTrack::Kind kind) {
    switch(kind) {
    case MediaTrack::Kind::AUDIO:
        out << "Audio";
        break;
    case MediaTrack::Kind::VIDEO:
        out << "Video";
        break;
    default:
        break;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, MediaTrack::Codec codec) {
    switch (codec)
    {
    case MediaTrack::Codec::H264:
        out << "H264";
        break;
    case MediaTrack::Codec::OPUS:
        out << "Opus";
        break;
    default:
        break;
    }
    return out;
}

} // namespace naivertc