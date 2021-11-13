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
    std::vector<FrameMap::iterator> frames_to_decode;
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

        // Filter the delta frames if the next required frame is key frame.
        if (keyframe_required_ && !frame.is_keyframe()) {
            continue;
        }
        
        auto last_decoded_frame_timestamp = decoded_frames_history_.last_decoded_frame_timestamp();
        // TODO: Consider removing this check as it may make a stream undecodable 
        // after a very long delay (multiple wrap arounds) between frames.
        if (last_decoded_frame_timestamp && 
            wrap_around_utils::AheadOf(*last_decoded_frame_timestamp, frame.timestamp())) {
            continue;
        }
        
        // TODO: Support spatial layer, and gather all remaining frames for the same super frame.
        if (frame.render_time_ms() == -1) {
            frame.set_render_time_ms(timing_->RenderTimeMs(frame.timestamp(), now_ms));
        }

        // Check the render time is valid or not, and reset the timing if necessary.
        if (!IsValidRenderTiming(frame.render_time_ms(), now_ms)) {
            jitter_estimator_.Reset();
            timing_->Reset();
            // Reset render time
            frame.set_render_time_ms(timing_->RenderTimeMs(frame.timestamp(), now_ms));
        }

        // The waiting time in ms before decoding this frame.
        wait_ms = timing_->MaxTimeWaitingToDecode(frame.render_time_ms(), now_ms);

        // This will happen when a slow decoder need a long time to decode,
        // wait_time_to_decode = wait_time_to_render - render_delay - decode_time.
        if (wait_ms < -kMaxAllowedFrameDelayMs) {
            // This will cause the frame buffer to prefer high framerate rather
            // than high resolution in the case of the decoder not decoding fast
            // enough and the stream has multiple spatial and temporal layers.
            // For multiple temporal layers it may cause non-base layer frames to be
            // skipped if they are late.
            continue;
        }

        // Found a decodable and required frame.
        keyframe_required_ = false;

        frames_to_decode.push_back(frame_it);

        break;

    } // end of for

    // No decodable frame found.
    if (frames_to_decode.empty()) {
        return;
    }

    ProcessFramesToDecode(std::move(frames_to_decode));
}

void FrameBuffer::ProcessFramesToDecode(std::vector<FrameMap::iterator> frames_to_decode) {
    if (frames_to_decode.empty()) {
        return;
    }
    int64_t now_ms = clock_->now_ms();
    int64_t wait_ms = 0;
    bool delayed_by_retransmission = false;
    uint32_t sent_timestamp = frames_to_decode[0]->second.frame.timestamp();
    int64_t received_time_ms = 0;
    int64_t render_time_ms = 0;
    size_t frame_size = 0;
    bool hand_off_decodability = false;

    for (auto& frame_it : frames_to_decode) {
        assert(frame_it != frames_.end());

        auto& frame = frame_it->second.frame;
        // Collect the infos of this batch of decodable frames.
        delayed_by_retransmission |= frame.delayed_by_retransmission();
        received_time_ms = std::max(received_time_ms, frame.received_time_ms());
        frame_size += frame.size();

        // Propagate the decodability to the dependent frames of this frame.
        hand_off_decodability = PropagateDecodability(frame_it->second);
        // Indicates the frame has beed decoded.
        decoded_frames_history_.InsertFrame(frame.id(), frame.timestamp());

        // Decode the current frame.
        if (frame_ready_to_decode_callback_) {
            // Limits the wait time in the range: [0, max_wait_ms]
            // wait_ms = std::min<int64_t>(wait_ms, max_wait_ms);
            // wait_ms = std::max<int64_t>(wait_ms, 0);
            wait_ms = timing_->MaxTimeWaitingToDecode(frame.render_time_ms(), now_ms);
            frame_ready_to_decode_callback_(std::move(frame_it->second.frame), wait_ms);
        }

        // Trigger state callback with all the dropped frames.
        if (auto observer = stats_observer_.lock()) {
            size_t dropped_frames = NumUndecodedFrames(frames_.begin(), frame_it);
            if (dropped_frames > 0) {
                observer->OnDroppedFrames(dropped_frames);
            }
        }

        // Remove this decoded frame and all undecodable frames before it.
        frames_.erase(frames_.begin(), ++frame_it);
    }

    // No nack has happened during transporting the batch frames,
    // and it can estimate the jitter delay directly.
    if (!delayed_by_retransmission) {
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

} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc