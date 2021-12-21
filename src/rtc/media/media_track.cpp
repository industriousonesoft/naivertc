#include "rtc/media/media_track.hpp"
#include "common/utils_random.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

MediaTrack::Kind ToKind(sdp::MediaEntry::Kind kind) {
    switch(kind) {
    case sdp::MediaEntry::Kind::AUDIO:
        return MediaTrack::Kind::AUDIO;
    case sdp::MediaEntry::Kind::VIDEO:
        return MediaTrack::Kind::VIDEO;
    default:
        return MediaTrack::Kind::UNKNOWN;
    }
}

// Video payload type range: [96, 111]
constexpr int kVideoPayloadTypeLowerRangeValue = 96;
constexpr int kVideoPayloadTypeUpperRangeValue = 111;
// Audio payload type range: [112, 127]
constexpr int kAudioPayloadTypeLowerRangeValue = 112;
constexpr int kAudioPayloadTypeUpperRangeValue = 127;

std::optional<int> NextPayloadType(MediaTrack::Kind kind) {
    if (kind == MediaTrack::Kind::AUDIO) {
        static int payload_type = kAudioPayloadTypeLowerRangeValue;
        if (payload_type + 1 <= kAudioPayloadTypeUpperRangeValue) {
            return payload_type++;
        } else {
            PLOG_WARNING << "No more payload type available for Audio codec";
            return std::nullopt;
        }
    } else if (kind == MediaTrack::Kind::VIDEO) {
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
    
} // namespace

// Media track
MediaTrack::MediaTrack(const Configuration& config) 
    : MediaChannel(config.kind(), config.mid()),
      description_(SdpBuilder::Build(config).value_or(sdp::Media())) {
}

MediaTrack::MediaTrack(sdp::Media description) 
    : MediaChannel(ToKind(description.kind()), description.mid()),
      description_(std::move(description)) {}

MediaTrack::~MediaTrack() {}

sdp::Media MediaTrack::description() const {
    return description_;
}

bool MediaTrack::Reconfig(const Configuration& config) {
    return task_queue_.Sync<bool>([this, &config](){
        if (config.kind() != kind_) {
            PLOG_WARNING << "Failed to reconfig as the incomming kind=" << config.kind()
                         << " is different from media track kind=" << kind_;
            return false;
        } else if (config.mid() != mid_) {
            PLOG_WARNING << "Failed to reconfig as the incomming mid=" << config.mid()
                         << " is different from local media mid=" << mid_;
            return false;
        }
        
        return true;
    });
}

// Private methods
void MediaTrack::Parse(const Configuration& config) {
    auto media_kind = config.kind();
    // Payload types
    // Media payload types
    config.ForEachCodec([&](const CodecParams& codec_params){
        auto payload_type = NextPayloadType(media_kind);
        if (payload_type) {
            media_codecs_[*payload_type] = codec_params.codec;
        }
    });

    // FEC
    if (config.fec_codec == FecCodec::ULP_FEC) {
        // ULP_FEC + RED
        red_payload_type_ = NextPayloadType(media_kind);
        fec_payload_type_ = NextPayloadType(media_kind);
    } else if (config.fec_codec == FecCodec::FLEX_FEC) {
        fec_payload_type_ = NextPayloadType(media_kind);
    }

    // RTX
    if (config.rtx_enabled) {
        // Protect media packet
        for (auto& kv : media_codecs_) {
            auto payload_type = NextPayloadType(media_kind);
            if (!payload_type) {
                break;
            }
            rtx_payload_type_map_[*payload_type] = kv.first;
        }
        // Protect RED packet
        auto payload_type = NextPayloadType(media_kind);
        if (red_payload_type_ && payload_type) {
            rtx_payload_type_map_[*payload_type] = *red_payload_type_;
        }
    }

    // SSRCs
    // Media stream
    media_ssrc_ = utils::random::generate_random<uint32_t>();
    // RTX stream
    if (config.rtx_enabled) {
        rtx_ssrc_ = utils::random::generate_random<uint32_t>();
    }
    // FlexFEC stream
    if (config.fec_codec == FecCodec::FLEX_FEC) {
        flex_fec_ssrc_ = utils::random::generate_random<uint32_t>();
    }
}
    
} // namespace naivertc