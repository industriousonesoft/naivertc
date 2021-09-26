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

constexpr int kDefaultVideoClockRate = 90000;

}

namespace naivertc {

std::optional<int> MediaTrack::NextPayloadType(Kind kind) {
    if (kind == Kind::AUDIO) {
        static int payload_type = kAudioPayloadTypeLowerRangeValue;
        if (payload_type + 1 <= kAudioPayloadTypeUpperRangeValue) {
            return payload_type++;
        }else {
            PLOG_WARNING << "No more payload type available for Audio codec";
            return std::nullopt;
        }
    }else if (kind == Kind::VIDEO) {
        static int payload_type = kVideoPayloadTypeLowerRangeValue;
        if (payload_type + 1 <= kVideoPayloadTypeUpperRangeValue) {
            return payload_type++;
        }else {
            PLOG_WARNING << "No more payload type available for Video codec";
            return std::nullopt;
        }
    }else {
        return std::nullopt;
    }
}

std::optional<sdp::Media> MediaTrack::CreateDescription(const MediaTrack::Configuration& config) {
    std::optional<sdp::Media> media = std::nullopt;
    if (config.kind == MediaTrack::Kind::VIDEO) {
        media.emplace(sdp::Media(sdp::MediaEntry::Kind::VIDEO,
                                 config.mid, 
                                 config.transport_protocols.value_or(kDefaultTransportPortocols), 
                                 sdp::Direction::SEND_ONLY));
    }else if (config.kind == MediaTrack::Kind::AUDIO) {
        media.emplace(sdp::Media(sdp::MediaEntry::Kind::AUDIO,
                                 config.mid,
                                 config.transport_protocols.value_or(kDefaultTransportPortocols), 
                                 sdp::Direction::SEND_ONLY));
    }
    return media;
}

bool MediaTrack::AddCodec(const CodecParams& cp) {
    return task_queue_.Sync<bool>([this, &cp](){
        if (kind_ == MediaTrack::Kind::AUDIO) {
            return AddAudioCodec(cp);
        }else if (kind_ == MediaTrack::Kind::VIDEO) {
            return AddVideoCodec(cp);
        }else {
            PLOG_WARNING << "Failed to add codec to a unknown kind media track.";
            return false;
        }
    });
}

bool MediaTrack::AddVideoCodec(const CodecParams& cp) {
    if (cp.codec == MediaTrack::Codec::H264) {
        // Associated payload types of RTX
        std::vector<int> associated_payload_types;
        // Codec: H264
        if (auto payload_type_opt = NextPayloadType(kind_)) {
            int payload_type = payload_type_opt.value();
            description_.AddVideoCodec(payload_type, "H264", cp.profile.value_or(kDefaultH264FormatProfile));
            // Feedback: NACK
            if (cp.nack_enabled) {
                description_.AddFeedback(payload_type, "nack");
                // TODO: Support more feedbacks
            }
            // Protected by RTX
            if (cp.rtx_enabled) {
                associated_payload_types.push_back(payload_type);
            }
        }else {
            PLOG_WARNING << "No more payload type for H264 codec";
        }
        
        // Codec: FEC
        if (cp.fec_codec.has_value()) {
            // ULP_FEC + RED
            if (cp.fec_codec.value() == FecCodec::ULP_FEC) {
                // Codec: RED
                if (auto payload_type_opt = NextPayloadType(kind_)) {
                    int payload_type = payload_type_opt.value();
                    description_.AddCodec(payload_type, "red", kDefaultVideoClockRate);
                    // Protected by RTX
                    if (cp.rtx_enabled) {
                        associated_payload_types.push_back(payload_type);
                    }
                }else {
                    PLOG_WARNING << "No more payload type for RED codec";
                }
                // Codec: ULP_FEC
                if (auto payload_type_opt = NextPayloadType(kind_)) {
                    int payload_type = payload_type_opt.value();
                    description_.AddCodec(payload_type, "ulpfec", kDefaultVideoClockRate);
                }else {
                    PLOG_WARNING << "No more payload type for ULP_FEC codec";
                }
            }
            // FlexFec + Ssrc
            else {
                // TODO: Build SDL line for FELX_FEC
            }
        }
        // Codec: RTX
        // See https://datatracker.ietf.org/doc/html/rfc4588#section-8
        if (cp.rtx_enabled) {
            for (const auto& apt : associated_payload_types) {
                if (auto payload_type = NextPayloadType(kind_)) {
                    // a=fmtp:<number> apt=<apt-value>;rtx-time=<rtx-time-val>
                    // TODO: Set rtx-time if necessary
                    description_.AddCodec(payload_type.value(), "rtx", kDefaultVideoClockRate, std::nullopt, "apt=" + std::to_string(apt));
                }else {
                    PLOG_WARNING << "No more payload type for RTX codec";
                }
            }
        }

        // ssrcs
        // Media ssrc
        // TODO: Add ssrc stream for sender
        // media->AddSsrc(utils::random::generate_random<uint32_t>(), 
        //                     sdp::Media::SsrcEntry::Kind::MEDIA, 
        //                     cp.cname, config.msid, config.track_id);
        // // RTX ssrc
        // if (config.rtx_enabled) {
        //     media_entry.AddSsrc(utils::random::generate_random<uint32_t>(), 
        //                         sdp::Media::SsrcEntry::Kind::RTX, 
        //                         config.cname, config.msid, config.track_id);
        // }
        // return media_entry;

        return true;
    }else {
        PLOG_WARNING << "Unsupported video codec: " << cp.codec;
        return false;
    }
}

bool MediaTrack::AddAudioCodec(const CodecParams& cp) {
    if (cp.codec == MediaTrack::Codec::OPUS) {
        // Codec: Opus 
        if (auto payload_type_opt = NextPayloadType(kind_)) {
            int payload_type = payload_type_opt.value();
            description_.AddAudioCodec(payload_type, "opus", 48000, 2, cp.profile.value_or(kDefaultOpusFormatProfile));
        }
        // TODO: Add feedback and FEC support.
        // ssrc
        // Media ssrc
        // TODO: Add ssrc stream for sender
        // media->AddSsrc(utils::random::generate_random<uint32_t>(), 
        //                     sdp::Media::SsrcEntry::Kind::MEDIA,
        //                     config.cname, config.msid, config.track_id);
        return true;
    }else {
        PLOG_WARNING << "Unsupported audio codec: " << cp.codec;
        return false;
    }
}

} // namespace naivertc
