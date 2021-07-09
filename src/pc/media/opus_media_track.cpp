#include "pc/media/opus_media_track.hpp"

namespace naivertc {

const std::string DEFAULT_OPUS_AUDIO_PROFILE =
    "minptime=10;maxaveragebitrate=96000;stereo=1;sprop-stereo=1;useinbandfec=1";

OpusMediaTrack::OpusMediaTrack(const Config& config, int sample_rate, int channels) 
    : MediaTrack(std::move(config)),
    sample_rate_(sample_rate),
    channels_(channels),
    description_(config_.mid) {
    for (int payload_type : config_.payload_types) {
        description_.AddCodec(payload_type, MediaTrack::codec_to_string(config_.codec), sample_rate_, channels, FormatProfileForPayloadType(payload_type));
    }
}

OpusMediaTrack::~OpusMediaTrack() {}

int OpusMediaTrack::sample_rate() const {
    return sample_rate_;
}

int OpusMediaTrack::channels() const {
    return channels_;
}

sdp::Media OpusMediaTrack::description() const {
    return description_;
}

std::optional<std::string> OpusMediaTrack::FormatProfileForPayloadType(int payload_type) const {
    if (payload_type == 111) {
        return DEFAULT_OPUS_AUDIO_PROFILE;
    }else {
        return std::nullopt;
    }
}
    
} // namespace naivert 

