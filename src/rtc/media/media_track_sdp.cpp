#include "rtc/media/media_track.hpp"
#include "rtc/sdp/sdp_media_entry_media.hpp"
#include "common/utils_random.hpp"

#include <plog/Log.h>

namespace naivertc {
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

sdp::Media MediaTrack::SdpBuilder::Build(const Configuration& config) {
    if (config.kind() == MediaTrack::Kind::AUDIO) {
        auto audio = sdp::Media(sdp::MediaEntry::Kind::AUDIO,
                                config.mid(),
                                kDefaultTransportPortocols, 
                                config.direction);
        AddCodecs(config, audio);;
        AddSsrcs(config, audio);
        return audio;
    } else if (config.kind() == MediaTrack::Kind::VIDEO) {
        auto video = sdp::Media(sdp::MediaEntry::Kind::VIDEO,
                                config.mid(),
                                kDefaultTransportPortocols, 
                                config.direction);
        AddCodecs(config, video);
        AddSsrcs(config, video);
        return video;
    } else {
        RTC_NOTREACHED();
    }
}

// Private methods
void MediaTrack::SdpBuilder::AddCodecs(const Configuration& config, sdp::Media& media) {
    // Associated payload types of RTX
    std::vector<int> associated_payload_types;
    auto kind = config.kind();
    // Media codecs
    config.ForEachCodec([&](const CodecParams& cp){
        // Media codec
        auto rtp_map = AddMediaCodec(NextPayloadType(kind), cp, media);
        if (rtp_map) {
            // Protected by RTX
            if (config.rtx_enabled) {
                rtp_map->rtx_payload_type = NextPayloadType(kind);
            }
            // RTCP feedbacks
            if (config.nack_enabled) {
                rtp_map->rtcp_feedbacks.push_back(sdp::Media::RtcpFeedback::NACK);
            }
            if (config.congestion_control) {
                // goog-cc
                if (config.congestion_control == CongestionControl::GOOG_REMB) {
                    rtp_map->rtcp_feedbacks.push_back(sdp::Media::RtcpFeedback::GOOG_REMB);
                // transport-cc
                } else {
                    rtp_map->rtcp_feedbacks.push_back(sdp::Media::RtcpFeedback::TRANSPORT_CC);
                }
            }
        } else {
            PLOG_WARNING << "Failed to add media codec: " << cp.codec;
        }
    });

    int clock_rate = config.kind() == MediaTrack::Kind::AUDIO ? kDefaultAudioSampleRate 
                                                              : kDefaultVideoClockRate;

    // FEC codec
    // ULP_FEC + RED
    if (config.fec_codec == FecCodec::ULP_FEC) {
        // Codec: RED
        auto red_rtp_map = media.AddCodec(NextPayloadType(kind), 
                                          sdp::Media::Codec::RED, 
                                          clock_rate);
        // Protected by RTX
        if (config.rtx_enabled) {
            red_rtp_map->rtx_payload_type = NextPayloadType(kind);
        }
        
        // Codec: ULP_FEC
        media.AddCodec(NextPayloadType(kind), 
                       sdp::Media::Codec::ULP_FEC, 
                       clock_rate);
    }
    // FlexFec + Ssrc
    else if (config.fec_codec == FecCodec::FLEX_FEC) {
        // Codec: FLEX_FEC
        media.AddCodec(NextPayloadType(kind), sdp::Media::Codec::FLEX_FEC, clock_rate);
    }

    // RTX Codec
    // a=fmtp:<number> apt=<apt-value>;rtx-time=<rtx-time-val>
    // See https://datatracker.ietf.org/doc/html/rfc4588#section-8
    if (config.rtx_enabled) {
        media.ForEachRtpMap([&](const sdp::Media::RtpMap& rtp_map){
            if (rtp_map.rtx_payload_type) {
                // TODO: Add rtx-time attribute if necessary
                // apt (Asssociated payload type)
                media.AddCodec(rtp_map.rtx_payload_type.value(), 
                               sdp::Media::Codec::RTX, 
                               clock_rate, 
                               /*codec_params=*/std::nullopt, 
                               "apt=" + std::to_string(rtp_map.payload_type));
            }
        });
    }
}

sdp::Media::RtpMap* MediaTrack::SdpBuilder::AddMediaCodec(int payload_type, 
                                                         const CodecParams& cp,
                                                         sdp::Media& media) {
    // Audio codecs
    // OPUS
    if (cp.codec == MediaTrack::Codec::OPUS) {
        sdp::Media::RtpMap rtp_map(payload_type, 
                                   sdp::Media::Codec::OPUS, 
                                   kDefaultAudioSampleRate, 
                                   /*codec_params=*/std::to_string(kDefaultAudioChannels));
        rtp_map.fmt_profiles.emplace_back(cp.profile.value_or(kDefaultOpusFormatProfile));
        return media.AddRtpMap(rtp_map);
    }
    // Video codecs
    // H264
    else if (cp.codec == MediaTrack::Codec::H264) {
        sdp::Media::RtpMap rtp_map(payload_type, 
                                   sdp::Media::Codec::H264, 
                                   kDefaultVideoClockRate);
        rtp_map.fmt_profiles.emplace_back(cp.profile.value_or(kDefaultH264FormatProfile));
        return media.AddRtpMap(rtp_map);
    }
    else {
        PLOG_WARNING << "Unsupported media codec: " << cp.codec;
        return nullptr;
    }
}

void MediaTrack::SdpBuilder::AddSsrcs(const Configuration& config, sdp::Media& media) {
    if (media.direction() != sdp::Direction::SEND_ONLY &&
        media.direction() != sdp::Direction::SEND_RECV) {
        PLOG_WARNING << "Inactive or only received media track do not contains send stream.";
        return;
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
}

int MediaTrack::SdpBuilder::NextPayloadType(Kind kind) {
    if (kind == Kind::AUDIO) {
        static int payload_type = kAudioPayloadTypeLowerRangeValue;
        if (payload_type + 1 <= kAudioPayloadTypeUpperRangeValue) {
            return payload_type++;
        } else {
            PLOG_WARNING << "No more payload type available for Audio codec";
            RTC_NOTREACHED();
        }
    } else if (kind == Kind::VIDEO) {
        static int payload_type = kVideoPayloadTypeLowerRangeValue;
        if (payload_type + 1 <= kVideoPayloadTypeUpperRangeValue) {
            return payload_type++;
        } else {
            PLOG_WARNING << "No more payload type available for Video codec";
            RTC_NOTREACHED();
        }
    } else {
        RTC_NOTREACHED();
    }
}
    
} // namespace naivertc
