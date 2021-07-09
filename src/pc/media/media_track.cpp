#include "pc/media/media_track.hpp"

namespace {

const std::string DEFAULT_H264_VIDEO_PROFILE =
    "profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1";
   
const std::string DEFAULT_OPUS_AUDIO_PROFILE =
    "minptime=10;maxaveragebitrate=96000;stereo=1;sprop-stereo=1;useinbandfec=1";
}

namespace naivertc {

MediaTrack::MediaTrack(const sdp::Media& description) 
    : description_(std::move(description)) {

}

std::string MediaTrack::mid() const {
    return description_.mid();
}

sdp::Direction MediaTrack::direction() const {
    return description_.direction();
}

sdp::Media MediaTrack::description() const {
    return description_;
}

void MediaTrack::UpdateDescription(sdp::Media description) {
    description_ = description;
}

std::string MediaTrack::kind_to_string(Kind kind) {
    switch (kind)
    {
    case Kind::VIDEO:
        return "video";
    case Kind::AUDIO:
        return "audio";
    }
}

std::string MediaTrack::codec_to_string(Codec codec) {
    switch (codec)
    {
    case Codec::H264:
        return "h264";
    case Codec::OPUS:
        return "opus";
    }
}

std::optional<std::string> MediaTrack::FormatProfileForPayloadType(int payload_type) {
    // audio
    if (payload_type == 111) {
        return DEFAULT_OPUS_AUDIO_PROFILE;
    }
    // video
    else if (payload_type == 102) {
        return DEFAULT_H264_VIDEO_PROFILE;
    }else {
        return std::nullopt;
    }
}

} // namespace naivertc