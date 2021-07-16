#include "rtc/sdp/sdp_media_entry_audio.hpp"

namespace naivertc {
namespace sdp {

/**
 * UDP/TLS/RTP/SAVPF：指明使用的传输协议，其中SAVPF是由S(secure)、F(feedback)、AVP（RTP A(audio)/V(video) profile, 详解rfc1890）组成
 * 即传输层使用UDP协议，并采用DTLS(UDP + TLS)，在传输层之上使用RTP(RTCP)协议，具体的RTP格式是SAVPF
 * 端口为9（可忽略，端口9为Discard Protocol专用），采用UDP传输加密的RTP包，并使用基于SRTCP的音视频反馈机制来提升传输质量
*/
Audio::Audio(std::string mid, Direction direction) 
    : Media("audio 9 UDP/TLS/RTP/SAVPF", std::move(mid), direction) {}

void Audio::AddCodec(int payload_type, std::string codec, int clock_rate, int channels, std::optional<std::string> profile) {
    RTPMap map(std::to_string(payload_type) + " " + codec + "/" + std::to_string(clock_rate) + "/" + std::to_string(channels));
    if (profile) {
        map.fmt_profiles.emplace_back(*profile);
    }
    AddRTPMap(map);
}

} // namespace sdp
} // namespace naivert 