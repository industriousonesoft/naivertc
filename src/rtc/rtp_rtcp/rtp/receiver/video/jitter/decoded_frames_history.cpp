#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/decoded_frames_history.hpp"

#include <algorithm>

#include <plog/Log.h>

namespace naivertc {
namespace rtc {
namespace video {

DecodedFramesHistory::DecodedFramesHistory(size_t window_size) 
    : window_size_(window_size),
      buffer_(window_size_) {}

DecodedFramesHistory::~DecodedFramesHistory() = default;

void DecodedFramesHistory::InsertFrame(int64_t frame_id, uint32_t timestamp) {
    last_decoded_frame_id_ = frame_id;
    last_decoded_frame_timestamp_ = timestamp;
    int new_index = FrameIdToIndex(frame_id);

    assert (last_frame_id_ < frame_id);

    // Clears expired values from the cyclic buffer.
    if (last_frame_id_) {
        int64_t frame_id_jump = frame_id - *last_frame_id_;
        int last_index = FrameIdToIndex(*last_frame_id_);
        if (frame_id_jump >= static_cast<int64_t>(window_size_)) {
            // Clear buffer if the jump of the missing frames is overflow.
            std::fill(buffer_.begin(), buffer_.end(), false);
        } else if (new_index > last_index) {
            // Reset missing frame range: 
            //  -> -> last_index+1 -> [ reset ] -> new_index - >
            // |                                                |
            // < - - - - - - - - - - - [ keep ] - - - - - - - - <
            std::fill(buffer_.begin() + last_index + 1, buffer_.begin() + new_index, false);
        } else {
            // Reset missing frame range: 
            //  -> -> last_index+1 -> [ keep ] -> new_index - - >
            // |                                                |
            // < - - - - - - - - - - - [ reset ] - - - - - - - -<
            std::fill(buffer_.begin() + last_index + 1, buffer_.end(), false);
            std::fill(buffer_.begin(), buffer_.begin() + new_index, false);
        }
    }

    buffer_[new_index] = true;
    last_frame_id_ = frame_id;
}

bool DecodedFramesHistory::WasDecoded(int64_t frame_id) {
    if (!last_frame_id_) {
        return false;
    }

    // Reference to the picture_id out of the stored should happen.
    // Cast the `size_t` type of `window_size_` to `int64_t` type to 
    // avoid the unexpected result as a negative result casted to a 
    // positive value implicitly.
    if (frame_id <= *last_frame_id_ - static_cast<int64_t>(window_size_)) {
        PLOG_WARNING << "Referencing a frame out of the window, "
                     << "assuming it was undecoded to avoid artifacts.";
        return false;
    }

    if (frame_id > last_frame_id_) {
        return false;
    }

    return buffer_[FrameIdToIndex(frame_id)];
}

void DecodedFramesHistory::Clear() {
    std::fill(buffer_.begin(), buffer_.end(), false);
    last_frame_id_.reset();
    last_decoded_frame_id_.reset();
    last_decoded_frame_timestamp_.reset();
}

// Private methods
int DecodedFramesHistory::FrameIdToIndex(int64_t frame_id) const {
    int index = frame_id % buffer_.size();
    return index >= 0 ? index : index + buffer_.size();
}

} // namespace video
} // namespace rtc
} // namespace naivert 