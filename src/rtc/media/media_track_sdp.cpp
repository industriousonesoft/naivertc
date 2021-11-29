#include "rtc/media/media_track.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "common/utils_random.hpp"

#include <plog/Log.h>

namespace {
// Video payload type range: [96, 111]
constexpr int kVideoPayloadTypeLowerRangeValue = 96;
constexpr int kVideoPayloadTypeUpperRangeValue = 111;
// Audio payload type range: [112, 127]
constexpr int kAudioPayloadTypeLowerRangeValue = 112;
constexpr int kAudioPayloadTypeUpperRangeValue = 127;

const std::string kDefaultH264FormatProfile =
    "profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1";
   
const std::string kDefaultOpusFormatProfile =
    "minptime=10;maxaveragebitrate=96000;stereo=1;sprop-stereo=1;useinbandfec=1";

/**
 * UDP/TLS/RTP/SAVPF：指明使用的传输协议，其中SAVPF是由S(secure)、F(feedback)、AVP（RTP A(audio)/V(video) profile, 详解rfc1890）组成
 * 即传输层使用UDP协议，并采用DTLS(UDP + TLS)，在传输层之上使用RTP(RTCP)协议，具体的RTP格式是SAVPF
 * 端口为9（可忽略，端口9为Discard Protocol专用），采用UDP传输加密的RTP包，并使用基于SRTCP的音视频反馈机制来提升传输质量
*/
const std::string kDefaultTransportPortocols = "UDP/TLS/RTP/SAVPF";

constexpr int kDefaultAudioChannels = 2;
constexpr int kDefaultAudioSampleRate = 48000;
constexpr int kDefaultVideoClockRate = 90000;

}

namespace naivertc {

std::optional<sdp::Media> MediaTrack::SdpBuilder::Build(const Configuration& config) {
    std::optional<sdp::Media> media = std::nullopt;
    if (config.kind() == MediaTrack::Kind::AUDIO) {
        auto audio = sdp::Media(sdp::MediaEntry::Kind::AUDIO,
                                config.mid(),
                                kDefaultTransportPortocols, 
                                config.direction);
        if (AddCodecs(config, audio) && AddSsrcs(config, audio)) {
            media.emplace(std::move(audio));
        }
    } else if (config.kind() == MediaTrack::Kind::VIDEO) {
        auto video = sdp::Media(sdp::MediaEntry::Kind::VIDEO,
                                config.mid(),
                                kDefaultTransportPortocols, 
                                config.direction);
        if (AddCodecs(config, video) && AddSsrcs(config, video)) {
            media.emplace(std::move(video));
        }
    }
    return media;
}

// Private methods
bool MediaTrack::SdpBuilder::AddCodecs(const Configuration& config, sdp::Media& media) {
    // Associated payload types of RTX
    std::vector<int> associated_payload_types;
    // Media codecs
    bool error_occured = false;
    config.ForEachCodec([&](const CodecParams& cp){
        if (auto payload_type_opt = NextPayloadType(config.kind())) {
            int payload_type = payload_type_opt.value();
            // Media codec
            AddMediaCodec(payload_type, cp, media);
            // Feedbacks
            config.ForEachFeedback([&](RtcpFeedback fb){
                AddFeedback(payload_type, fb, media);
            });
            // Protected by RTX
            if (config.rtx_enabled) {
                associated_payload_types.push_back(payload_type);
            }
        } else {
            PLOG_WARNING << "No more payload type for video codec:" << cp.codec;
            error_occured = true;
            return;
        }
    });
    if (error_occured) {
        return false;
    }

    int clock_rate = config.kind() == MediaTrack::Kind::AUDIO ? kDefaultAudioSampleRate : kDefaultVideoClockRate;
    // FEC codec
    // ULP_FEC + RED
    if (config.fec_codec == FecCodec::ULP_FEC) {
        // Codec: RED
        if (auto payload_type_opt = NextPayloadType(config.kind())) {
            int payload_type = payload_type_opt.value();
            media.AddCodec(payload_type, "red", clock_rate);
            // Protected by RTX
            if (config.rtx_enabled) {
                associated_payload_types.push_back(payload_type);
            }
        } else {
            PLOG_WARNING << "No more payload type for RED codec";
            return false;
        }
        // Codec: ULP_FEC
        if (auto payload_type = NextPayloadType(config.kind())) {
            media.AddCodec(payload_type.value(), "ulpfec", clock_rate);
        } else {
            PLOG_WARNING << "No more payload type for ULP_FEC codec";
            return false;
        }
    }
    // FlexFec + Ssrc
    else if (config.fec_codec == FecCodec::FLEX_FEC) {
        // Codec: FLEX_FEC
        if (auto payload_type = NextPayloadType(config.kind())) {
            media.AddCodec(payload_type.value(), "flexfec", clock_rate);
        } else {
            PLOG_WARNING << "No more payload type for ULP_FEC codec";
            return false;
        }
        // TODO: Does the FLEX_FEC be protected by RTX?
    }

    // RTX codex
    // a=fmtp:<number> apt=<apt-value>;rtx-time=<rtx-time-val>
    // See https://datatracker.ietf.org/doc/html/rfc4588#section-8
    if (config.rtx_enabled) {
        for (const auto& apt : associated_payload_types) {
            if (auto payload_type = NextPayloadType(config.kind())) {
                // TODO: Add rtx-time attribute if necessary
                media.AddCodec(payload_type.value(), "rtx", clock_rate, std::nullopt, "apt=" + std::to_string(apt));
            } else {
                PLOG_WARNING << "No more payload type for RTX codec";
                return false;
            }
        }
    }
    return true;

}

bool MediaTrack::SdpBuilder::AddMediaCodec(int payload_type, const CodecParams& cp, sdp::Media& media) {
    // Audio codecs
    // OPUS
    if (cp.codec == MediaTrack::Codec::OPUS) {
        media.AddAudioCodec(payload_type, "opus", kDefaultAudioSampleRate, kDefaultAudioChannels, cp.profile.value_or(kDefaultOpusFormatProfile));
        return true;
    }
    // Video codecs
    // H264
    else if (cp.codec == MediaTrack::Codec::H264) {
        media.AddVideoCodec(payload_type, "H264", cp.profile.value_or(kDefaultH264FormatProfile));
        return true;
    }
    else {
        PLOG_WARNING << "Unsupported media codec: " << cp.codec;
        return false;
    }
}

bool MediaTrack::SdpBuilder::AddFeedback(int payload_type, RtcpFeedback fb, sdp::Media& media) {
    switch(fb) {
    case RtcpFeedback::NACK:
        media.AddFeedback(payload_type, "nack");
        break;
    default:
        break;
    }
    return true;
}

bool MediaTrack::SdpBuilder::AddSsrcs(const Configuration& config, sdp::Media& media) {
    if (media.direction() != sdp::Direction::SEND_ONLY &&
        media.direction() != sdp::Direction::SEND_RECV) {
        PLOG_WARNING << "Inactive or only received media track do not contains send stream.";
        return false;
    }
    // Media ssrc
    media.AddSsrc(utils::random::generate_random<uint32_t>(), 
                  sdp::Media::SsrcEntry::Kind::MEDIA, 
                  config.cname, config.msid, config.track_id);
    // RTX ssrc
    if (config.rtx_enabled) {
        media.AddSsrc(utils::random::generate_random<uint32_t>(), 
                      sdp::Media::SsrcEntry::Kind::RTX, 
                      config.cname, config.msid, config.track_id);
    }
    // FlexFEC ssrc
    if (config.fec_codec == FecCodec::FLEX_FEC) {
        media.AddSsrc(utils::random::generate_random<uint32_t>(), 
                      sdp::Media::SsrcEntry::Kind::FEC, 
                      config.cname, config.msid, config.track_id);
    }
    return true;
}

std::optional<int> MediaTrack::SdpBuilder::NextPayloadType(Kind kind) {
    if (kind == Kind::AUDIO) {
        static int payload_type = kAudioPayloadTypeLowerRangeValue;
        if (payload_type + 1 <= kAudioPayloadTypeUpperRangeValue) {
            return payload_type++;
        } else {
            PLOG_WARNING << "No more payload type available for Audio codec";
            return std::nullopt;
        }
    } else if (kind == Kind::VIDEO) {
        static int payload_type = kVideoPayloadTypeLowerRangeValue;
        if (payload_type + 1 <= kVideoPayloadTypeUpperRangeValue) {
            return payload_type++;
        } else {
            PLOG_WARNING << "No more payload type available for Video codec";
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
}
    
} // namespace naivertc
