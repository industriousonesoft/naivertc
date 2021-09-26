#include "rtc/media/media_track.hpp"

namespace naivertc {
namespace {
MediaChannel::Kind MediaKindFromDescription(const sdp::Media& description) {
    switch (description.kind()) {
    case sdp::MediaEntry::Kind::AUDIO:
        return MediaChannel::Kind::AUDIO;
    case sdp::MediaEntry::Kind::VIDEO:
        return MediaChannel::Kind::VIDEO;
    default:
        return MediaChannel::Kind::UNKNOWN;
    }
}

} // namespace

// Configuration
MediaTrack::Configuration::Configuration::Configuration(Kind kind, 
                                                        std::string mid,
                                                        std::optional<std::string> transport_protocols,
                                                        std::optional<std::string> cname,
                                                        std::optional<std::string> msid,
                                                        std::optional<std::string> track_id) 
    : kind(kind),
      mid(std::move(mid)),
      transport_protocols(std::move(transport_protocols)),
      cname(std::move(cname)),
      msid(std::move(msid)),
      track_id(std::move(track_id)) {}

// CodecParams
MediaTrack::CodecParams::CodecParams(Codec codec, 
                                     bool nack_enabled, 
                                     bool rtx_enabled, 
                                     std::optional<FecCodec> fec_codec, 
                                     std::optional<std::string> profile) 
    : codec(codec),
      nack_enabled(nack_enabled),
      rtx_enabled(rtx_enabled),
      fec_codec(std::move(fec_codec)),
      profile(std::move(profile)) {}

// Media track
MediaTrack::MediaTrack(sdp::Media description) 
    : MediaChannel(MediaKindFromDescription(description), description.mid()),
      description_(std::move(description)) {}

MediaTrack::~MediaTrack() {}

sdp::Direction MediaTrack::direction() const {
    return task_queue_.Sync<sdp::Direction>([this](){
        return description_.direction();
    });
}

const sdp::Media* MediaTrack::description() const {
    return task_queue_.Sync<const sdp::Media*>([this](){
        return &description_;
    });
}

void MediaTrack::set_description(sdp::Media description) {
    task_queue_.Async([this, description=std::move(description)](){
        description_ = std::move(description);
    });
    
}

// Overload operator<<
std::ostream& operator<<(std::ostream& out, MediaTrack::Codec codec) {
    switch (codec) {
    case MediaTrack::Codec::H264:
        out << "H264";
        break;
    case MediaTrack::Codec::OPUS:
        out << "Opus";
        break;
    default:
        break;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, MediaTrack::FecCodec codec) {
    switch (codec) {
    case MediaTrack::FecCodec::ULP_FEC:
        out << "ULP_FEC";
        break;
    case MediaTrack::FecCodec::FLEX_FEC:
        out << "FLEX_FEC";
        break;
    default:
        break;
    }
    return out;
}

} // namespace naivertc