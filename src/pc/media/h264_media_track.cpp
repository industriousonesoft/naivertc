#include "pc/media/h264_media_track.hpp"

namespace naivertc {

const std::string DEFAULT_H264_VIDEO_PROFILE =
    "profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1";

H264MediaTrack::H264MediaTrack(Config config) 
    : MediaTrack(std::move(config)),
    payload_type_(0) {

}

H264MediaTrack::~H264MediaTrack() {}

int H264MediaTrack::payload_type() const {
    return payload_type_;
}

void H264MediaTrack::set_payload_type(int payload_type) {
    payload_type_ = payload_type;
}

std::optional<std::string> H264MediaTrack::format_profile() const {
    // FIXME: payload type 与 format profile是一对一的关系吗？ 
    if (payload_type_ == 102) {
        return DEFAULT_H264_VIDEO_PROFILE;
    }else {
        return std::nullopt;
    }
}
    
} // namespace naivertc
