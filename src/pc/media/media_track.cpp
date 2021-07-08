#include "pc/media/media_track.hpp"

namespace naivertc {

MediaTrack::MediaTrack(Config config) 
    : config_(std::move(config)) {

}

std::string MediaTrack::kind_string() const {
    switch (kind())
    {
    case Kind::VIDEO:
        return "video";
    case Kind::AUDIO:
        return "audio";
    }
}

std::string MediaTrack::codec_string() const {
    switch (codec())
    {
    case Codec::H264:
        return "h264";
    case Codec::OPUS:
        return "opus";
    }
}

} // namespace naivertc