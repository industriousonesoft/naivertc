#include "rtc/sdp/sdp_media_entry_media.hpp"

namespace naivertc {
namespace sdp {

void Media::AddVideoCodec(int payload_type, 
                     const std::string codec, 
                     std::optional<const std::string> profile) {
    RtpMap rtp_map(payload_type, codec, 90000);
    if (profile.has_value())
        rtp_map.fmt_profiles.emplace_back(profile.value());

    AddRtpMap(rtp_map);
}

} // namespace sdp
} // namespace naivert 