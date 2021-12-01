#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtp {
namespace video {
namespace jitter {
namespace {

// The time it's allowed for a frame to be late to its rendering prediction and
// still be rendered.
constexpr int kMaxAllowedFrameDelayMs = 5;

// The max target delay in ms for a frame to be rendered.
constexpr int64_t kMaxVideoDelayMs = 10000; // 10s

} // namespace

void FrameBuffer::NextFrame(int64_t max_wait_time_ms, 
                            bool keyframe_required,
                            std::function<void(std::optional<video::FrameToDecode>)> callback) {
    RTC_RUN_ON(decode_queue_);
    int64_t last_return_time_ms = clock_->now_ms() + max_wait_time_ms;
    std::lock_guard lock(lock_);
    waiting_deadline_ms_ = last_return_time_ms;
    keyframe_required_ = keyframe_required;
    next_frame_found_callback_ = std::move(callback);
    StartWaitForNextFrameToDecode();
}

// Private methods
void FrameBuffer::StartWaitForNextFrameToDecode() {
    RTC_RUN_ON(decode_queue_);
    assert(!decode_task_ || !decode_task_->Running());
    int64_t wait_ms = FindNextFrameToDecode();
    decode_task_ = RepeatingTask::DelayedStart(clock_, decode_queue_, TimeDelta::Millis(wait_ms), [this]() {
        std::optional<video::FrameToDecode> next_frame = std::nullopt;
        NextFrameFoundCallback callback = nullptr;
        {
            std::lock_guard lock(lock_);
            if (frame_to_decode_) {
                next_frame = GetNextFrameToDecode();
            } else if (clock_->now_ms() < waiting_deadline_ms_) {
                // If there's no frames to decode and there is still time left, 
                // we should continue waiting for the remaining time.
                return TimeDelta::Millis(FindNextFrameToDecode());
            }
            callback = std::move(next_frame_found_callback_);
            next_frame_found_callback_ = nullptr;
        }
        callback(std::move(next_frame));
        return TimeDelta::Zero();
    });
}

int64_t FrameBuffer::FindNextFrameToDecode() {
    RTC_RUN_ON(decode_queue_);
    int64_t now_ms = clock_->now_ms();
    const int64_t max_wait_time_ms = waiting_deadline_ms_ - now_ms;
    int64_t wait_time_ms = max_wait_time_ms;
    frame_to_decode_.reset();

    for (auto frame_it = frame_infos_.begin(); 
         frame_it != frame_infos_.end() && frame_it->first <= last_continuous_frame_id_; 
         ++frame_it) {

        auto& frame_info = frame_it->second;

        // Filter the decoded frames.
        if (frame_info.frame == std::nullopt) {
            continue;
        }

        // Filter the uncontinuous or undecodable frames.
        if (!frame_info.continuous() || 
            frame_info.num_missing_decodable > 0) {
            continue;
        }

        // Filter the delta frames if the next frame we need is key frame.
        if (keyframe_required_ && !frame_info.frame->is_keyframe()) {
            continue;
        }
        
        // Filter the undecodable frames by timestamp.
        auto last_decoded_frame_timestamp = decoded_frames_history_.last_decoded_frame_timestamp();
        if (last_decoded_frame_timestamp && 
            wrap_around_utils::AheadOf(*last_decoded_frame_timestamp, frame_info.frame->timestamp())) {
            PLOG_WARNING << "Frame (id=" << frame_info.frame->id()
                         << ") can not be decoded as the frames behind it have been decoded.";
            continue;
        }
        
        // TODO: Gather and combine all remaining frames for the same superframe.

        // Retrieve the decodable frame.
        frame_to_decode_.emplace(frame_it);
        
        // Set render time if necessary.
        if (frame_info.frame->render_time_ms() == -1) {
            // Set a estimated render time that we expect.
            frame_info.frame->set_render_time_ms(timing_->RenderTimeMs(frame_info.frame->timestamp(), now_ms));
        }

        // Check if the render time is valid or not, and reset the timing if necessary.
        if (!IsValidRenderTiming(frame_info.frame->render_time_ms(), now_ms)) {
            jitter_estimator_.Reset();
            timing_->Reset();
            // Reset the render time.
            frame_info.frame->set_render_time_ms(timing_->RenderTimeMs(frame_info.frame->timestamp(), now_ms));
        }

        // The waiting time in ms before decoding this frame:
        // wait_ms = render_time_ms - now_ms - decode_time_ms - render_delay_ms
        wait_time_ms = timing_->MaxWaitingTimeBeforeDecode(frame_info.frame->render_time_ms(), now_ms);

        // Drop the frame in case of the decoder is not decoding fast enough (spend a long time to decode) or
        // the stream has multiple spatial and temporal layers (spend a long to wait all layers to complete super frame).
        // NOTE: This will cause the frame buffer to prefer high framerate rather than high resolution.
        if (wait_time_ms < -kMaxAllowedFrameDelayMs) {
            // Try to find the next frame to decode in the remained frames.
            // NOTE: In other word, we will decode the current frame if it's the last decodable frame so far.
            continue;
        }
        
        break;
    } // end of for

    // Limits the wait time in the range: [0, max_wait_time_ms]
    wait_time_ms = std::min<int64_t>(wait_time_ms, max_wait_time_ms);
    wait_time_ms = std::max<int64_t>(wait_time_ms, 0);
    return wait_time_ms;
}

video::FrameToDecode FrameBuffer::GetNextFrameToDecode() {
    RTC_RUN_ON(decode_queue_);
    assert(frame_to_decode_);

    auto& frame_info_it = frame_to_decode_.value();
    auto& frame_info = frame_info_it->second;
    assert(frame_info.frame);
    
    // Update related info.
    // Propagate the decodability to the dependent frames of this frame.
    PropagateDecodability(frame_info);
    // Indicate the frame was decoded.
    decoded_frames_history_.InsertFrame(frame_info.frame->id(), frame_info.frame->timestamp());

    // Trigger state callback with all the dropped frames.
    if (auto observer = stats_observer_.lock()) {
        size_t dropped_frames = NumUndecodableFrames(frame_infos_.begin(), frame_info_it);
        if (dropped_frames > 0) {
            observer->OnDroppedFrames(dropped_frames);
        }
    }
    // Retrieve the next frame to decode.
    video::FrameToDecode frame = std::move(frame_info.frame.value());
    // Remove decoded frame and all undecoded frames before it.
    frame_infos_.erase(frame_infos_.begin(), ++frame_info_it);

    // No nack has happened during the transport of this frame,
    // and it can estimate the jitter delay directly.
    if (!frame.delayed_by_retransmission()) {
        // Estimate the jitter occurred during the transport of this frame.
        int jitter_delay_ms = EstimateJitterDelay(frame.timestamp(), /* send timestamp */
                                                  frame.received_time_ms(), /* arrived time */
                                                  frame.size());
        timing_->set_jitter_delay_ms(jitter_delay_ms);
        timing_->UpdateCurrentDelay(frame.render_time_ms(), clock_->now_ms() /* actual_decode_time_ms */);
    } else {
        if (add_rtt_to_playout_delay_) {
            jitter_estimator_.FrameNacked();
        }
    }

    // TODO: Update jitter delay and timing frame info.
    // TODO: Return next frames to decode

    // NOTE: Deliver the frame to decode at last.
    return frame;
}

bool FrameBuffer::IsValidRenderTiming(int64_t render_time_ms, int64_t now_ms) {
    RTC_RUN_ON(decode_queue_);
    // Zero render time means render immediately.
    if (render_time_ms == 0) {
        return true;
    }
    if (render_time_ms < 0) {
        return false;
    }
    // Delay before the frame to be rendered.
    int64_t delay_ms = abs(render_time_ms - now_ms);
    if (delay_ms > kMaxVideoDelayMs) {
        PLOG_WARNING << "A frame about to be decoded is out of the configured"
                     << " delay bounds (" << delay_ms << " > " << kMaxVideoDelayMs
                     << "). Resetting the video jitter buffer.";
        return false;
    }
    int target_delay_ms = timing_->TargetDelayMs();
    if (target_delay_ms > kMaxVideoDelayMs) {
        PLOG_WARNING << "The video target delay=" << target_delay_ms 
                     << " has grown larger than the max video delay="
                     << kMaxVideoDelayMs << " ms.";
        return false;
    }
    return true;
}

// NOTE: This function MUST be called after the frame was decoded to 
// make sure the dependent frames of it can be decoded later.
int64_t FrameBuffer::PropagateDecodability(const FrameInfo& frame_info) {
    int64_t last_decodable_frame_id = -1;
    for (int64_t frame_id : frame_info.dependent_frames) {
        auto dep_frame_it = frame_infos_.find(frame_id);
        // Make sure the dependent frame is still in the buffer, since
        // the older frames will be removed after a jump in frame id happened.
        if (dep_frame_it != frame_infos_.end() && dep_frame_it->second.num_missing_decodable > 0) {
            --dep_frame_it->second.num_missing_decodable;
            if (dep_frame_it->second.num_missing_decodable == 0) {
                last_decodable_frame_id = frame_id;
            }
        }
    }
    return last_decodable_frame_id;
}

int FrameBuffer::EstimateJitterDelay(uint32_t send_timestamp, int64_t recv_time_ms, size_t frame_size) {
    RTC_RUN_ON(decode_queue_);
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