#include "rtc/media/video/common.hpp"

namespace naivertc {
namespace video {

std::ostream& operator<<(std::ostream& out, FrameType type) {
    switch (type) {
    case video::FrameType::EMPTY:
        out << "Empty";
        break;
    case video::FrameType::KEY:
        out << "Key";
        break;
    case video::FrameType::DELTA:
        out << "Delta";
        break;
    default:
        break;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, CodecType type) {
    switch (type) {
    case video::CodecType::GENERIC:
        out << "Generic";
        break;
    case video::CodecType::H264:
        out << "H264";
        break;
    default:
        break;
    }
    return out;
}

} // namespace video
} // namespace naivertc