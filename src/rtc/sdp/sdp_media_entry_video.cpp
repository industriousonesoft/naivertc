#include "rtc/sdp/sdp_media_entry_video.hpp"

namespace naivertc {
namespace sdp {

Video::Video(std::string mid, Direction direction) 
    : Media(Type::VIDEO, std::move(mid), "UDP/TLS/RTP/SAVPF", direction) {}

Video::~Video() {}

void Video::AddCodec(int payload_type, const std::string codec, std::optional<std::string> profile) {
    RTPMap map(std::to_string(payload_type) + " " + std::move(codec) + "/90000");
    // TODO: Replace fixed feedback settings with input parameters
    map.AddFeedback("nack");
    map.AddFeedback("nack pli");
    map.AddFeedback("goog-remb");
    if (profile)
        map.fmt_profiles.emplace_back(*profile);

    AddRTPMap(map);
}

} // namespace sdp
} // namespace naivert 