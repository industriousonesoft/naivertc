#include "rtc/media/media_track.hpp"
#include "rtc/sdp/sdp_media_entry_audio.hpp"
#include "rtc/sdp/sdp_media_entry_video.hpp"
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

std::optional<sdp::Media> MediaTrack::BuildDescription(const MediaTrack::Configuration& config) {
    auto codec = config.codec;
    auto kind = config.kind;
    if (kind == MediaTrack::Kind::VIDEO) {
        if (codec == MediaTrack::Codec::H264) {
            auto media_entry = sdp::Video(config.mid);
            // Associated payload types of RTX
            std::vector<int> associated_payload_types;
            // Codec: H264
            if (auto payload_type_opt = NextPayloadType(kind)) {
                int payload_type = payload_type_opt.value();
                media_entry.AddCodec(payload_type, "H264", kDefaultOpusFormatProfile);
                // Feedback: NACK
                if (config.nack_enabled) {
                    media_entry.AddFeedback(payload_type, "nack");
                    // TODO: Support more feedbacks
                }
                // Protected by RTX
                if (config.rtx_enabled) {
                    associated_payload_types.push_back(payload_type);
                }
            }else {
                PLOG_WARNING << "No more payload type for H264 codec";
            }
            
            // Codec: FEC
            if (config.fec_codec.has_value()) {
                // ULP_FEC + RED
                if (config.fec_codec.value() == FecCodec::ULP_FEC) {
                    // Codec: RED
                    if (auto payload_type_opt = NextPayloadType(kind)) {
                        int payload_type = payload_type_opt.value();
                        media_entry.AddCodec(payload_type, "red");
                        // Protected by RTX
                        if (config.rtx_enabled) {
                            associated_payload_types.push_back(payload_type);
                        }
                    }else {
                        PLOG_WARNING << "No more payload type for RED codec";
                    }
                    // Codec: ULP_FEC
                    if (auto payload_type_opt = NextPayloadType(kind)) {
                        int payload_type = payload_type_opt.value();
                        media_entry.AddCodec(payload_type, "ulpfec");
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
            if (config.rtx_enabled) {
                for (const auto& apt : associated_payload_types) {
                    if (auto payload_type = NextPayloadType(kind)) {
                        // a=fmtp:<number> apt=<apt-value>;rtx-time=<rtx-time-val>
                        // TODO: Set rtx-time if necessary
                        media_entry.AddCodec(payload_type.value(), "rtx", "apt=" + std::to_string(apt));
                    }else {
                        PLOG_WARNING << "No more payload type for RTX codec";
                    }
                }
            }

            // ssrcs
            // Media ssrc
            sdp::Media::SsrcEntry ssrc_entry(utils::random::generate_random<uint32_t>(), config.cname, config.msid, config.track_id);
            media_entry.AddSsrcEntry(ssrc_entry);
            // RTX ssrc
            if (config.rtx_enabled) {
                ssrc_entry.ssrc = utils::random::generate_random<uint32_t>();
                media_entry.AddSsrcEntry(ssrc_entry);
            }
            return media_entry;
        }else {
            PLOG_WARNING << "Unsupported video codec: " << codec;
            return std::nullopt;
        }
    }else if (kind == MediaTrack::Kind::AUDIO) {
        if (codec == MediaTrack::Codec::OPUS) {
            auto media_entry = sdp::Audio(config.mid);
            // Codec: Opus 
            if (auto payload_type_opt = NextPayloadType(kind)) {
                int payload_type = payload_type_opt.value();
                media_entry.AddCodec(payload_type, "opus", 48000, 2, kDefaultH264FormatProfile);
            }
            // TODO: Add feedback and FEC support.
            // ssrc
            // Media ssrc
            sdp::Media::SsrcEntry ssrc_entry(utils::random::generate_random<uint32_t>(), config.cname, config.msid, config.track_id);
            media_entry.AddSsrcEntry(ssrc_entry);
            return media_entry;
        }else {
            PLOG_WARNING << "Unsupported audio codec: " << codec;
            return std::nullopt;
        }
    }else {
        PLOG_WARNING << "Unsupported media kind: " << kind;
        return std::nullopt;
    }
}

} // namespace naivertc
