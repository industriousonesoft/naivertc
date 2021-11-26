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

// FrameInfo
FrameBuffer::FrameInfo::FrameInfo() = default;
FrameBuffer::FrameInfo::~FrameInfo() = default;

// FrameBuffer
FrameBuffer::FrameBuffer(std::shared_ptr<Clock> clock, 
                         std::shared_ptr<Timing> timing,
                         std::shared_ptr<TaskQueue> decode_queue,
                         std::weak_ptr<VideoReceiveStatisticsObserver> stats_observer)
    : clock_(std::move(clock)),
      timing_(std::move(timing)),
      decode_queue_(std::move(decode_queue)),
      stats_observer_(std::move(stats_observer)),
      decoded_frames_history_(kMaxFramesHistory),
      jitter_estimator_({/* Default HyperParameters */}, clock_),
      protection_mode_(ProtectionMode::NACK),
      add_rtt_to_playout_delay_(true),
      last_log_non_decoded_ms_(-kLogNonDecodedIntervalMs) {
    assert(clock_ != nullptr);
    assert(timing_ != nullptr);
}

FrameBuffer::~FrameBuffer() {}

void FrameBuffer::Clear() {
    std::lock_guard lock(lock_);
    ClearFramesAndHistory();
}

void FrameBuffer::UpdateRtt(int64_t rtt_ms) {
    std::lock_guard lock(lock_);
    jitter_estimator_.UpdateRtt(rtt_ms);
}

ProtectionMode FrameBuffer::protection_mode() const {
    return protection_mode_;
}

void FrameBuffer::set_protection_mode(ProtectionMode mode) {
    std::lock_guard lock(lock_);
    protection_mode_ = mode;
}

// Private methods
void FrameBuffer::ClearFramesAndHistory() {
    if (auto observer = stats_observer_.lock()) {
        // The undecodable frames
        size_t dropped_frames = NumUndecodableFrames(frame_infos_.begin(), frame_infos_.end());
        if (dropped_frames > 0) {
            PLOG_WARNING << "Dropped " << dropped_frames << " frames";
            observer->OnDroppedFrames(dropped_frames);
        }
    }
    frame_infos_.clear();
    frame_to_decode_.reset();
    last_continuous_frame_id_.reset();
    decoded_frames_history_.Clear();
}

size_t FrameBuffer::NumUndecodableFrames(FrameInfoMap::iterator begin, FrameInfoMap::iterator end) {
    return std::count_if(begin, end, 
                         [](const std::pair<const int64_t, FrameInfo>& frame_tuple) {
        return frame_tuple.second.frame != std::nullopt;
    });
}

} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc