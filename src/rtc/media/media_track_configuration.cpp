#include "rtc/media/media_track.hpp"

#include <plog/Log.h>

namespace naivertc {

// CodecParams
MediaTrack::CodecParams::CodecParams(Codec codec,
                                     std::optional<std::string> profile) 
    : codec(codec),
      profile(std::move(profile)) {}

// Configuration
MediaTrack::Configuration::Configuration::Configuration(Kind kind, 
                                                        std::string mid) 
    : kind_(kind),
      mid_(std::move(mid)) {}


bool MediaTrack::Configuration::AddCodec(CodecParams cp) {
    if (kind_ == MediaTrack::Kind::AUDIO) {
        if (cp.codec == Codec::OPUS) {
            media_codecs_.push_back(std::move(cp));
            return true;
        } else {
            PLOG_WARNING << "Unsupported audio codec: " << cp.codec;
            return false;
        }
    } else if (kind_ == MediaTrack::Kind::VIDEO) {
        if (cp.codec == Codec::H264) {
            media_codecs_.push_back(std::move(cp));
            return true;
        } else {
            PLOG_WARNING << "Unsupported video codec: " << cp.codec;
            return false;
        }
    } else {
        PLOG_WARNING << "Failed to add codec: " << cp.codec 
                     << "to an unknown kind media track.";
        return false;
    }
}

bool MediaTrack::Configuration::AddCodec(Codec codec, std::optional<std::string> profile) {
    return AddCodec(CodecParams(codec, profile));
}

void MediaTrack::Configuration::RemoveCodec(Codec codec, std::optional<std::string> profile) {
    for (auto it = media_codecs_.begin(); it != media_codecs_.end();) {
        if (it->codec == codec && it->profile == profile) {
            it = media_codecs_.erase(it);
        } else {
            ++it;
        }
    }
}

void MediaTrack::Configuration::ForEachCodec(std::function<void(const CodecParams& cp)>&& handler) const {
    for (const auto& cp : media_codecs_) {
        handler(cp);
    }
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
