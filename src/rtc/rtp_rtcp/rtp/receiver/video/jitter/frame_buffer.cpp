#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer.hpp"

#include <plog/Log.h>

#include <algorithm>

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {
namespace {

// Max number of frames the buffer will hold.
constexpr size_t kMaxFramesBuffered = 800;
    
} // namespace


FrameBuffer::FrameBuffer() {}
FrameBuffer::~FrameBuffer() {}

int64_t FrameBuffer::InsertFrame(std::unique_ptr<video::FrameToDecode> frame) {
    int64_t last_continuous_frame_id = last_continuous_frame_id_.value_or(-1);

    if (!ValidReferences(*(frame.get()))) {
        PLOG_WARNING << "Frame " << frame->id()
                     << " has invaild frame reference, dropping it.";
        return last_continuous_frame_id;
    }

    if (frame_buffer_.size() >= kMaxFramesBuffered) {
        if (frame->is_keyframe()) {
            PLOG_WARNING << "Inserting keyframe " << frame->id()
                         << " but the buffer is full, clearing buffer and inserting the frame.";
        } else {

        }
    }

    return -1;
}

void FrameBuffer::Clear() {
    unsigned int dropped_frames = std::count_if(frame_buffer_.begin(), frame_buffer_.end(), 
                                                [](const std::pair<const int64_t, FrameInfo>& frame){
        return frame.second.frame != nullptr;
    });
    if (dropped_frames > 0) {
        PLOG_WARNING << "Dropped " << dropped_frames << " frames";
    }
    frame_buffer_.clear();
    last_continuous_frame_id_.reset();
    frame_to_decode_.clear();
    // TODO: Clear decoded frames history.
}

// Private methods
bool FrameBuffer::ValidReferences(const video::FrameToDecode& frame) {
    if (frame.frame_type() == VideoFrameType::KEY) {
        // Key frame has no reference.
        return frame.NumReferences() == 0;
    } else {
        bool is_valid = true;
        frame.ForEachReference([&is_valid, &frame](int64_t ref_picture_id, bool* stoped) {
            // The frame id a the B frame, droping it.
            if (ref_picture_id >= frame.id()) {
                is_valid = false;
                *stoped = true;
            }
        });
        return is_valid;
    }
}
    
} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivertc