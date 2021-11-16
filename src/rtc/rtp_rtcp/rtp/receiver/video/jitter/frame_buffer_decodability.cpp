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

// Private methods
void FrameBuffer::FindNextDecodableFrames() {
    // NOTE: Using vector to support the temporal scalability in the future.
    std::vector<FrameInfo> frames_to_decode;
    int64_t wait_ms = 0;
    int64_t now_ms = clock_->now_ms();

    for (auto frame_it = frames_.begin(); 
        frame_it != frames_.end() && 
        frame_it->first <= last_continuous_frame_id_;
        ++frame_it) {

        // Filter the invalid frame.
        if (!frame_it->second.IsValid()) {
            continue;
        }

        // Filter the uncontinuous or undecodable frames.
        if (!frame_it->second.continuous() || 
            frame_it->second.num_missing_decodable != 0) {
            continue;
        }

        auto& frame_info = frame_it->second;
        auto& frame = frame_info.frame;

        // Filter the delta frames if the next frame we need is key frame.
        if (keyframe_required_ && !frame.is_keyframe()) {
            continue;
        }
        
        // Filter the undecodable frames by timestamp.
        auto last_decoded_frame_timestamp = decoded_frames_history_.last_decoded_frame_timestamp();
        if (last_decoded_frame_timestamp && 
            wrap_around_utils::AheadOf(*last_decoded_frame_timestamp, frame.timestamp())) {
            PLOG_WARNING << "Frame (id=" << frame.id()
                         << ") can not be decoded as the frames behind it have been decoded.";
            continue;
        }
        
        // TODO: Gather all remaining frames for the same superframe.

        // Found a decodable and required frame.
        keyframe_required_ = false;

        // Trigger state callback with all the dropped frames.
        if (auto observer = stats_observer_.lock()) {
            size_t dropped_frames = NumUndecodedFrames(frames_.begin(), frame_it);
            if (dropped_frames > 0) {
                observer->OnDroppedFrames(dropped_frames);
            }
        }

        // Move the current frame to `frames_to_decode`.
        frames_to_decode.push_back(std::move(frame_it->second));

        // Remove this decoded frame and all undecodable frames before it.
        frames_.erase(frames_.begin(), ++frame_it);

        break;

    } // end of for

    // No decodable frame found.
    if (frames_to_decode.empty()) {
        return;
    }

    // Decode frames for the same superframe in decode queue.
    decode_queue_->Async([this, frames=std::move(frames_to_decode)](){
        DecodeFrames(std::move(frames));
    });
}

void FrameBuffer::DecodeFrames(std::vector<FrameInfo> frames_to_decode) {
    if (frames_to_decode.empty()) {
        return;
    }
    auto& first_frame = frames_to_decode[0].frame;
    int64_t now_ms = clock_->now_ms();
    int64_t wait_ms = 0;

    // Set render time if necessary.
    if (first_frame.render_time_ms() == -1) {
        // Set a estimated render time that we expect.
        first_frame.set_render_time_ms(timing_->RenderTimeMs(first_frame.timestamp(), now_ms));
    }

    // Check if the render time is valid or not, and reset the timing if necessary.
    if (!IsValidRenderTiming(first_frame.render_time_ms(), now_ms)) {
        jitter_estimator_.Reset();
        timing_->Reset();
        // Reset the render time.
        first_frame.set_render_time_ms(timing_->RenderTimeMs(first_frame.timestamp(), now_ms));
    }

    // The waiting time in ms before decoding this frame:
    // wait_ms = render_time_ms - now_ms - decode_time_ms - render_delay_ms
    wait_ms = timing_->MaxWaitingTimeBeforeDecode(first_frame.render_time_ms(), now_ms);

    // This will cause the frame buffer to prefer high framerate rather
    // than high resolution in two case:
    // 1). the decoder is not decoding fast enough;
    // 2). the stream has multiple spatial and temporal layers.
    // For multiple temporal layers it may cause non-base layer frames to be
    // skipped if they are late.
    if (wait_ms < -kMaxAllowedFrameDelayMs) {
        return;
    }

    uint32_t sent_timestamp = first_frame.timestamp();
    int64_t render_time_ms = first_frame.render_time_ms();
    bool delayed_by_retransmission = false;
    int64_t received_time_ms = 0;
    size_t frame_size = 0;
    bool hand_off_decodability = false;

    // All frames for the same superframe
    for (auto& frame_info : frames_to_decode) {
        auto& frame = frame_info.frame;

        // Set render time if necessary.
        if (frame.render_time_ms() == -1) {
            // Set a estimated render time that we expect.
            frame.set_render_time_ms(timing_->RenderTimeMs(frame.timestamp(), now_ms));
        }

        // Collect the infos of this batch of decodable frames.
        delayed_by_retransmission |= frame.delayed_by_retransmission();
        received_time_ms = std::max(received_time_ms, frame.received_time_ms());
        frame_size += frame.size();

        // TODO: The two method should be called in task queue.
        // Propagate the decodability to the dependent frames of this frame.
        hand_off_decodability = PropagateDecodability(frame_info);
        // Indicates the frame was decoded.
        decoded_frames_history_.InsertFrame(frame.id(), frame.timestamp());

        // Decode the current frame.
        if (frame_ready_to_decode_callback_) {
            // Limits the wait time in the range: [0, max_wait_ms]
            // wait_ms = std::min<int64_t>(wait_ms, max_wait_ms);
            // wait_ms = std::max<int64_t>(wait_ms, 0);
            wait_ms = timing_->MaxWaitingTimeBeforeDecode(render_time_ms, now_ms);
            frame_ready_to_decode_callback_(std::move(frame_info.frame), wait_ms);
        }
    }

    // No nack has happened during the transport of this frame,
    // and it can estimate the jitter delay directly.
    if (!delayed_by_retransmission) {
        // Estimate the jitter occurred during the transport of this frame.
        int jitter_delay_ms = EstimateJitterDelay(sent_timestamp, received_time_ms, frame_size);
        timing_->set_jitter_delay_ms(jitter_delay_ms);
        timing_->UpdateCurrentDelay(render_time_ms, now_ms);
    } else {
        if (add_rtt_to_playout_delay_) {
            jitter_estimator_.FrameNacked();
        }
    }

    // TODO: Update jitter delay and timing frame info.
    // TODO: Return next frames to decode

    // Has new dacodable frames probably,
    // and schedule to find them later.
    if (hand_off_decodability) {
        task_queue_->Async([this](){
            FindNextDecodableFrames();
        });
    }
}

bool FrameBuffer::IsValidRenderTiming(int64_t render_time_ms, int64_t now_ms) {
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

bool FrameBuffer::PropagateDecodability(const FrameInfo& frame_info) {
    bool has_decodable_frame = false;
    for (int64_t frame_id : frame_info.dependent_frames) {
        auto dep_frame_it = frames_.find(frame_id);
        // Make sure the dependent frame is still in the buffer, since
        // the older frames will be removed after a jump in frame id happened.
        if (dep_frame_it != frames_.end() && dep_frame_it->second.num_missing_decodable > 0) {
            --dep_frame_it->second.num_missing_decodable;
            if (dep_frame_it->second.num_missing_decodable == 0) {
                has_decodable_frame = true;
            }
        }
    }
    return has_decodable_frame;
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