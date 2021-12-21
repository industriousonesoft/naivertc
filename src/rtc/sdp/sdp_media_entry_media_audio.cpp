#include "rtc/sdp/sdp_media_entry_media.hpp"

namespace naivertc {
namespace sdp {

void Media::AddAudioCodec(int payload_type, 
                          Codec codec, 
                          int clock_rate, 
                          int channels, 
                          std::optional<const std::string> profile) {
    RtpMap rtp_map(payload_type, codec, clock_rate, std::to_string(channels));
    if (profile.has_value()) {
        rtp_map.fmt_profiles.emplace_back(profile.value());
    }
    AddRtpMap(rtp_map);
}

} // namespace sdp
} // namespace naivert 