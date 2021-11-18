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
    decode_queue_->Async([this, max_wait_time_ms, keyframe_required, callback=std::move(callback)](){
        waiting_deadline_ms_ = clock_->now_ms() + max_wait_time_ms;
        keyframe_required_ = keyframe_required;
        next_frame_found_callback_ = std::move(callback);
        StartWaitForNextFrameToDecode();
    });
}

// Private methods
void FrameBuffer::FindNextDecodableFrames() {
    assert(task_queue_->is_in_current_queue());
    // NOTE: Using vector to support the temporal scalability in the future.
    std::optional<video::FrameToDecode> frame_to_decode = std::nullopt;

    for (auto frame_it = frame_infos_.begin(); frame_it != frame_infos_.end(); ++frame_it) {

        // TODO: Using optional frame instead of last_decodable_frame_id_.
        // Filter the frame info with invalid id.
        if (frame_it->first > last_continuous_frame_id_ || 
            frame_it->first <= last_decodable_frame_id_) {
            continue;
        } 

        // Filter the frame info without a frame.
        if (frame_it->second.frame == std::nullopt) {
            continue;
        }

        // Filter the uncontinuous or undecodable frames.
        if (!frame_it->second.continuous() || 
            frame_it->second.num_missing_decodable != 0) {
            continue;
        }
        
        auto& frame = frame_it->second.frame;
        assert(frame_it->first == frame->id());
   
        // Filter the undecodable frames by timestamp.
        auto last_decoded_frame_timestamp = decoded_frames_history_.last_decoded_frame_timestamp();
        if (last_decoded_frame_timestamp && 
            wrap_around_utils::AheadOf(*last_decoded_frame_timestamp, frame->timestamp())) {
            PLOG_WARNING << "Frame (id=" << frame->id()
                         << ") can not be decoded as the frames behind it have been decoded.";
            continue;
        }
        
        // TODO: Gather and combine all remaining frames for the same superframe.

        last_decodable_frame_id_.emplace(frame->id());
        // Keep the found frame to decode later.
        frame_to_decode.swap(frame_it->second.frame);

        break;

    } // end of for

    // No decodable frame found.
    if (!frame_to_decode.has_value()) {
        return;
    }

    // Try to decode frame in decode queue.
    decode_queue_->Async([this, frame=std::move(frame_to_decode.value())](){
        decodable_frames_.push_back(std::move(frame));
        // Check if the decode task has started.
        if (decode_repeating_task_) {
             // Do nothing if the decode task was done yet.
            if (!decode_repeating_task_->Running()) {
                return;
            }
            // The decode task is waiting for next frame to decode,
            // so we restart it for new decodable frame.
            decode_repeating_task_->Stop();
            StartWaitForNextFrameToDecode();
        }
    });
}

void FrameBuffer::StartWaitForNextFrameToDecode() {
    assert(decode_queue_->is_in_current_queue());
    assert(!decode_repeating_task_ || !decode_repeating_task_->Running());
    int64_t wait_ms = FindNextFrameToDecode();
    decode_repeating_task_ = RepeatingTask::DelayedStart(clock_, decode_queue_, TimeDelta::Millis(wait_ms), [this]() {
        if (!decodable_frames_.empty()) {
            next_frame_found_callback_(GetNextFrameToDecode());
        } else if (clock_->now_ms() < waiting_deadline_ms_) {
            // If there's no frames to decode and there is still time left, 
            // we should continue waiting for the remaining time.
            return TimeDelta::Millis(FindNextFrameToDecode());
        } else {
            // No frame found and timeout.
            next_frame_found_callback_(std::nullopt);
        }
        return TimeDelta::Zero();
    });
}

int64_t FrameBuffer::FindNextFrameToDecode() {
    assert(decode_queue_->is_in_current_queue());
    int64_t now_ms = clock_->now_ms();
    int64_t max_wait_time_ms = waiting_deadline_ms_ - now_ms;
    int64_t wait_time_ms = max_wait_time_ms;
    for (auto frame_it = decodable_frames_.begin(); frame_it != decodable_frames_.end(); ++frame_it) {
        // Filter the delta frames if the next frame we need is key frame.
        if (keyframe_required_ && !frame_it->is_keyframe()) {
            continue;
        }

        // Set render time if necessary.
        if (frame_it->render_time_ms() == -1) {
            // Set a estimated render time that we expect.
            frame_it->set_render_time_ms(timing_->RenderTimeMs(frame_it->timestamp(), now_ms));
        }

        // Check if the render time is valid or not, and reset the timing if necessary.
        if (!IsValidRenderTiming(frame_it->render_time_ms(), now_ms)) {
            jitter_estimator_.Reset();
            timing_->Reset();
            // Reset the render time.
            frame_it->set_render_time_ms(timing_->RenderTimeMs(frame_it->timestamp(), now_ms));
        }

        // The waiting time in ms before decoding this frame:
        // wait_ms = render_time_ms - now_ms - decode_time_ms - render_delay_ms
        wait_time_ms = timing_->MaxWaitingTimeBeforeDecode(frame_it->render_time_ms(), now_ms);

        // This will cause the frame buffer to prefer high framerate rather
        // than high resolution in two case:
        // 1). the decoder is not decoding fast enough;
        // 2). the stream has multiple spatial and temporal layers.
        // For multiple temporal layers it may cause non-base layer frames to be
        // skipped if they are late.
        if (max_wait_time_ms < -kMaxAllowedFrameDelayMs) {
            continue;
        }

        // Remove all undecoded frames before the found frame excluding itself.
        decodable_frames_.erase(decodable_frames_.begin(), frame_it);

        // Ready to decode the first frame in `decodable_frames_`.
        break;
    }
    // Limits the wait time in the range: [0, max_wait_time_ms]
    wait_time_ms = std::min<int64_t>(wait_time_ms, max_wait_time_ms);
    wait_time_ms = std::max<int64_t>(wait_time_ms, 0);
    return wait_time_ms;
}

video::FrameToDecode FrameBuffer::GetNextFrameToDecode() {
    assert(decode_queue_->is_in_current_queue());
    assert(decodable_frames_.size() > 0);
    video::FrameToDecode frame = std::move(*decodable_frames_.begin());
    decodable_frames_.pop_front();
    
    int64_t frame_id = frame.id();
    uint32_t timestamp = frame.timestamp();
    int64_t received_time_ms = frame.received_time_ms();
    size_t frame_size = frame.size();
    int64_t render_time_ms = frame.render_time_ms();
    bool delayed_by_retransmission = frame.delayed_by_retransmission();
    
    // No nack has happened during the transport of this frame,
    // and it can estimate the jitter delay directly.
    if (!delayed_by_retransmission) {
        // Estimate the jitter occurred during the transport of this frame.
        int jitter_delay_ms = EstimateJitterDelay(timestamp, received_time_ms, frame_size);
        timing_->set_jitter_delay_ms(jitter_delay_ms);
        timing_->UpdateCurrentDelay(render_time_ms, clock_->now_ms() /* actual_decode_time_ms */);
    } else {
        if (add_rtt_to_playout_delay_) {
            jitter_estimator_.FrameNacked();
        }
    }

    // TODO: Update jitter delay and timing frame info.
    // TODO: Return next frames to decode

    // Update `frame_infos_` after decoded.
    task_queue_->Async([this, frame_id, timestamp](){
        // Indicate the frame was decoded.
        decoded_frames_history_.InsertFrame(frame_id, timestamp);
        // Retrieve the info of the decoded frame.
        auto frame_info_it = frame_infos_.find(frame_id);
        assert(frame_info_it != frame_infos_.end());
        // Propagate the decodability to the dependent frames of this frame.
        bool has_decodable_frame = PropagateDecodability(frame_info_it->second);
        // Trigger state callback with all the dropped frames.
        if (auto observer = stats_observer_.lock()) {
            size_t dropped_frames = NumUndecodedFrames(frame_infos_.begin(), frame_info_it);
            if (dropped_frames > 0) {
                observer->OnDroppedFrames(dropped_frames);
            }
        }
        // Remove decoded frame and all undecoded frames before it.
        frame_infos_.erase(frame_infos_.begin(), ++frame_info_it);
        if (has_decodable_frame) {
            // Detected new decodable frames.
            FindNextDecodableFrames();
        }
    });

    // NOTE: Deliver the frame to decode after updating the relative infos.
    return frame;
}

bool FrameBuffer::IsValidRenderTiming(int64_t render_time_ms, int64_t now_ms) {
    assert(decode_queue_->is_in_current_queue());
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
bool FrameBuffer::PropagateDecodability(const FrameInfo& frame_info) {
    assert(task_queue_->is_in_current_queue());
    bool has_decodable_frame = false;
    for (int64_t frame_id : frame_info.dependent_frames) {
        auto dep_frame_it = frame_infos_.find(frame_id);
        // Make sure the dependent frame is still in the buffer, since
        // the older frames will be removed after a jump in frame id happened.
        if (dep_frame_it != frame_infos_.end() && dep_frame_it->second.num_missing_decodable > 0) {
            --dep_frame_it->second.num_missing_decodable;
            if (dep_frame_it->second.num_missing_decodable == 0) {
                has_decodable_frame = true;
            }
        }
    }
    return has_decodable_frame;
}

int FrameBuffer::EstimateJitterDelay(uint32_t send_timestamp, int64_t recv_time_ms, size_t frame_size) {
    assert(decode_queue_->is_in_current_queue());
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