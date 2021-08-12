#include "rtc/media/media_track.hpp"
#include "rtc/sdp/sdp_media_entry_audio.hpp"
#include "rtc/sdp/sdp_media_entry_video.hpp"

namespace {

const std::string DEFAULT_H264_VIDEO_PROFILE =
    "profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1";
   
const std::string DEFAULT_OPUS_AUDIO_PROFILE =
    "minptime=10;maxaveragebitrate=96000;stereo=1;sprop-stereo=1;useinbandfec=1";

}

namespace naivertc {

sdp::Media MediaTrack::BuildDescription(const MediaTrack::Configuration& config) {
    auto codec = config.codec;
    auto kind = config.kind;
    if (kind == MediaTrack::Kind::VIDEO) {
        if (codec == MediaTrack::Codec::H264) {
            auto description = sdp::Video(config.mid);
            // payload types
            for (auto payload_type : config.payload_types) {
                description.AddCodec(payload_type, MediaTrack::codec_to_string(codec), MediaTrack::FormatProfileForPayloadType(payload_type));
            }
            // ssrc
            description.AddSSRC(config.ssrc, config.cname, config.msid, config.track_id);
            return std::move(description);
        }else {
            throw std::invalid_argument("Unsupported video codec: " + MediaTrack::codec_to_string(codec));
        }
    }else if (kind == MediaTrack::Kind::AUDIO) {
        if (codec == MediaTrack::Codec::OPUS) {
            auto description = sdp::Audio(config.mid);
            // payload types
            for (int payload_type : config.payload_types) {
                // TODO: Not use fixed sample rate and channel value
                description.AddCodec(payload_type, MediaTrack::codec_to_string(codec), 48000, 2, MediaTrack::FormatProfileForPayloadType(payload_type));
            }
            // ssrc
            description.AddSSRC(config.ssrc, config.cname, config.msid, config.track_id);
            return std::move(description);
        }else {
            throw std::invalid_argument("Unsupported audio codec: " + MediaTrack::codec_to_string(codec));
        }
    }else {
        throw std::invalid_argument("Unsupported media kind: " + MediaTrack::kind_to_string(kind));
    }
}

std::string MediaTrack::kind_to_string(Kind kind) {
    switch (kind)
    {
    case Kind::VIDEO:
        return "video";
    case Kind::AUDIO:
        return "audio";
    }
}

std::string MediaTrack::codec_to_string(Codec codec) {
    switch (codec)
    {
    case Codec::H264:
        return "h264";
    case Codec::OPUS:
        return "opus";
    }
}

std::optional<std::string> MediaTrack::FormatProfileForPayloadType(int payload_type) {
    // audio
    if (payload_type == 111) {
        return DEFAULT_OPUS_AUDIO_PROFILE;
    }
    // video
    else if (payload_type == 102) {
        return DEFAULT_H264_VIDEO_PROFILE;
    }else {
        return std::nullopt;
    }
}

} // namespace naivertc
