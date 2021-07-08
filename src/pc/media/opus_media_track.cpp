#include "pc/media/opus_media_track.hpp"

namespace naivertc {

const std::string DEFAULT_OPUS_AUDIO_PROFILE =
    "minptime=10;maxaveragebitrate=96000;stereo=1;sprop-stereo=1;useinbandfec=1";

OpusMediaTrack::OpusMediaTrack(Config config, int sample_rate, int channels) 
    : MediaTrack(std::move(config)),
    payload_type_(0),
    sample_rate_(sample_rate),
    channels_(channels) {}

OpusMediaTrack::~OpusMediaTrack() {}

int OpusMediaTrack::sample_rate() const {
    return sample_rate_;
}

int OpusMediaTrack::channels() const {
    return channels_;
}

int OpusMediaTrack::payload_type() const {
    return payload_type_;
}

void OpusMediaTrack::set_payload_type(int payload_type) {
    payload_type_ = payload_type;
}

std::optional<std::string> OpusMediaTrack::format_profile() const {
    if (payload_type_ == 111) {
        return DEFAULT_OPUS_AUDIO_PROFILE;
    }else {
        return std::nullopt;
    }
}
    
} // namespace naivert 

