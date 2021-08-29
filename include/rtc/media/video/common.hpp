#ifndef _RTC_MEDIA_VIDEO_COMMON_H_
#define _RTC_MEDIA_VIDEO_COMMON_H_

#include "base/defines.hpp"

namespace naivertc {
namespace video {

// Frame types
enum class FrameType {
    EMPTTY,
    KEY,
    DELTA
};

// Codec types
enum class CodecType {
    NONE,
    H264,
    VP8,
    VP9,
};

// Playout delay
// Minimum and maximum playout delay values from capture to render.
// These are best effort values.
//
// A value < 0 indicates no change from previous valid value.
//
// min = max = 0 indicates that the receiver should try and render
// frame as soon as possible.
//
// min = x, max = y indicates that the receiver is free to adapt
// in the range (x, y) based on network jitter.
struct RTC_CPP_EXPORT PlayoutDelay {
    PlayoutDelay() = default;
    PlayoutDelay(int min_ms, int max_ms) : min_ms(min_ms), max_ms(max_ms) {}
    int min_ms = -1;
    int max_ms = -1;

    bool operator==(const PlayoutDelay& rhs) const {
        return min_ms == rhs.min_ms && max_ms == rhs.max_ms;
    }

    bool IsAvailable() const { return min_ms >= 0 || max_ms >= 0; }
};
    
} // namespace video
} // namespace naivertc


#endif