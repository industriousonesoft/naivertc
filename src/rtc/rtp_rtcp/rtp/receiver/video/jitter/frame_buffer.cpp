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
FrameBuffer::FrameInfo::FrameInfo(video::FrameToDecode frame) : frame(std::move(frame)) {}
FrameBuffer::FrameInfo::FrameInfo(FrameInfo&&) = default;
FrameBuffer::FrameInfo::~FrameInfo() = default;

// FrameBuffer
FrameBuffer::FrameBuffer(ProtectionMode protection_mode,
                         std::shared_ptr<Clock> clock, 
                         std::shared_ptr<Timing> timing,
                         std::shared_ptr<TaskQueue> task_queue,
                         std::weak_ptr<VideoReceiveStatisticsObserver> stats_observer)
    : protection_mode_(protection_mode),
      clock_(std::move(clock)),
      timing_(std::move(timing)),
      task_queue_(std::move(task_queue)),
      stats_observer_(std::move(stats_observer)),
      decoded_frames_history_(kMaxFramesHistory),
      jitter_estimator_({/* Default HyperParameters */}, clock_),
      keyframe_required_(true /* Require a keyframe to start */),
      add_rtt_to_playout_delay_(true),
      last_log_non_decoded_ms_(-kLogNonDecodedIntervalMs) {
    assert(clock_ != nullptr);
    assert(timing_ != nullptr);
    assert(task_queue_ != nullptr);
}

FrameBuffer::~FrameBuffer() {}

void FrameBuffer::Clear() {
    task_queue_->Async([this](){
        if (auto observer = stats_observer_.lock()) {
            size_t dropped_frames = NumUndecodedFrames(frames_.begin(), frames_.end());
            if (dropped_frames > 0) {
                PLOG_WARNING << "Dropped " << dropped_frames << " frames";
                observer->OnDroppedFrames(dropped_frames);
            }
        }
        frames_.clear();
        last_continuous_frame_id_.reset();
        decoded_frames_history_.Clear();
    });
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

void FrameBuffer::RequireKeyframe() {
    task_queue_->Async([this](){
        keyframe_required_ = true;
        // Try to find a decadable keyframe.
        FindNextDecodableFrames();
    });
}

void FrameBuffer::OnDecodableFrame(FrameReadyToDecodeCallback callback) {
    task_queue_->Async([this, callback=std::move(callback)](){
        frame_ready_to_decode_callback_ = std::move(callback);
    });
}

// Private methods
size_t FrameBuffer::NumUndecodedFrames(FrameMap::iterator begin, FrameMap::iterator end) {
    return std::count_if(begin, end, 
                         [](const std::pair<const int64_t, FrameInfo>& frame_tuple) {
        return frame_tuple.second.IsValid();
    });
}

int FrameBuffer::EstimateJitterDelay(uint32_t send_timestamp, int64_t recv_time_ms, size_t frame_size) {
    // Calculate the delay of the current frame from the previous frame.
    auto [frame_delay, success] = inter_frame_delay_.CalculateDelay(send_timestamp, recv_time_ms);
    if (success) {
        assert(frame_delay >= 0);
        jitter_estimator_.UpdateEstimate(frame_delay, frame_size);
    }

    // `rtt_mult` will be 1 if protection mode is NACK only.
    float rtt_mult = protection_mode_ == ProtectionMode::NACK_FEC ? 0 : 1;
    std::optional<float> rtt_mult_add_cap_ms = std::nullopt;
    // TODO: Enable RttMultExperiment if necessary.

    return jitter_estimator_.GetJitterEstimate(rtt_mult, rtt_mult_add_cap_ms);
}

} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc