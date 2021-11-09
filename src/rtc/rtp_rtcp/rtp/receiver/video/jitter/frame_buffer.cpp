#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

#include <plog/Log.h>

#include <algorithm>
#include <queue>

namespace naivertc {
namespace rtp {
namespace video {
namespace jitter {
namespace {

// Max number of decoded frame info that will be saved.
constexpr int kMaxFramesHistory = 1 << 13; // 8192
    
} // namespace


FrameBuffer::FrameBuffer(std::shared_ptr<Clock> clock, 
                         std::shared_ptr<Timing> timing,
                         std::shared_ptr<TaskQueue> task_queue)
    : clock_(std::move(clock)),
      timing_(std::move(timing)),
      task_queue_(std::move(task_queue)),
      decoded_frames_history_(kMaxFramesHistory),
      jitter_estimator_({/* Default HyperParameters */}, clock_),
      keyframe_required_(true /* Require a keyframe to start */),
      add_rtt_to_playout_delay_(true),
      latest_render_time_ms_(-1),
      last_log_non_decoded_ms_(-kLogNonDecodedIntervalMs),
      protection_mode_(ProtectionMode::NACK) {
    assert(clock_ != nullptr);
    assert(timing_ != nullptr);
    assert(task_queue_ != nullptr);
}

FrameBuffer::~FrameBuffer() {}

void FrameBuffer::Clear() {
    size_t dropped_frames = NumUndecodedFrames(frames_.begin(), frames_.end());
    if (dropped_frames > 0) {
        PLOG_WARNING << "Dropped " << dropped_frames << " frames";
    }
    frames_.clear();
    last_continuous_frame_id_.reset();
    frames_to_decode_.clear();
    decoded_frames_history_.Clear();
}

void FrameBuffer::UpdateRtt(int64_t rtt_ms) {
    task_queue_->Async([this, rtt_ms](){
        jitter_estimator_.UpdateRtt(rtt_ms);
    });
}

ProtectionMode FrameBuffer::protection_mode() const {
    return task_queue_->Sync<ProtectionMode>([this](){
        return protection_mode_;
    });
}

void FrameBuffer::set_protection_mode(ProtectionMode mode) {
    task_queue_->Async([this, mode](){
        protection_mode_ = mode; 
    });
}

bool FrameBuffer::keyframe_required() const {
    return task_queue_->Sync<bool>([this](){
        return keyframe_required_;
    });
}

void FrameBuffer::set_keyframe_required(bool keyframe_required) {
    task_queue_->Async([this, keyframe_required](){
        keyframe_required_ = keyframe_required;
    });
}

// Private methods
size_t FrameBuffer::NumUndecodedFrames(FrameMap::iterator begin, FrameMap::iterator end) {
    return std::count_if(begin, end, 
                         [](const std::pair<const int64_t, FrameInfo>& frame_tuple) {
        return frame_tuple.second.frame != nullptr;
    });
}

} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc