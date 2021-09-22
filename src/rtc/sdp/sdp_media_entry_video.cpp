#include "rtc/sdp/sdp_media_entry_video.hpp"

namespace naivertc {
namespace sdp {

Video::Video(std::string mid, Direction direction) 
    : Media(Type::VIDEO, std::move(mid), "UDP/TLS/RTP/SAVPF", direction) {}

Video::~Video() {}

void Video::AddCodec(int payload_type, const std::string codec, std::optional<const std::string> profile) {
    RtpMap rtp_map;
    rtp_map.payload_type = payload_type;
    rtp_map.codec = codec;
    rtp_map.clock_rate = 90000;
    rtp_map.codec_params = std::nullopt;

    if (profile.has_value())
        rtp_map.fmt_profiles.emplace_back(profile.value());

    AddRtpMap(rtp_map);
}

} // namespace sdp
} // namespace naivert 