#include "pc/media/media_track.hpp"

namespace naivertc {

MediaTrack::MediaTrack(const Config& config) 
    : config_(std::move(config)) {

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

} // namespace naivertc