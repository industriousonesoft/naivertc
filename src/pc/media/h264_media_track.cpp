#include "pc/media/h264_media_track.hpp"

namespace naivertc {

const std::string DEFAULT_H264_VIDEO_PROFILE =
    "profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1";

H264MediaTrack::H264MediaTrack(const Config& config) 
    : MediaTrack(std::move(config)),
    description_(config_.mid) {
    for (auto payload_type : payload_types()) {
        description_.AddCodec(payload_type, MediaTrack::codec_to_string(config_.codec), FormatProfileForPayloadType(payload_type));
    }
}

H264MediaTrack::~H264MediaTrack() {}

sdp::Media H264MediaTrack::description() const {
    return description_;
}

std::optional<std::string> H264MediaTrack::FormatProfileForPayloadType(int payload_type) const {
    // FIXME: payload type 与 format profile是一对一的关系吗？ 
    if (payload_type == 102) {
        return DEFAULT_H264_VIDEO_PROFILE;
    }else {
        return std::nullopt;
    }
}
    
} // namespace naivertc
