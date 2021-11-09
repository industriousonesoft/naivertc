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
                            std::shared_ptr<TaskQueue> task_queue, 
                            NextFrameCallback callback) {
    // The render time of next decoded frame.
    latest_render_time_ms_ = clock_->now_ms() + max_wait_time_ms;
    keyframe_required_ = keyframe_required;
    // TODO: Determing whether assiging the `task_queue` here or not?
}

// Private methods
FrameBuffer::FrameMap::iterator FrameBuffer::FindNextDecodableFrame() {
    FrameMap::iterator next_decodable_frame = frames_.end();

    for (auto frame_it = frames_.begin(); 
        frame_it != frames_.end() && frame_it->first <= last_continuous_frame_id_; 
        ++frame_it) {

        // Filter the frames is uncontinuous or undecodable.
        if (!frame_it->second.continuous() || frame_it->second.num_missing_decodable > 0) {
            continue;
        }

        auto last_decoded_frame_timestamp = decoded_frames_history_.last_decoded_frame_timestamp();
        // TODO: Consider removing this check as it may make a stream undecodable 
        // after a very long delay (multiple wrap arounds) between frames.
        if (last_decoded_frame_timestamp && 
            wrap_around_utils::AheadOf(*last_decoded_frame_timestamp, frame_it->second.frame->timestamp())) {
            PLOG_WARNING << "A jump in frame timestamp was detected, this may make frame with id: " 
                         << frame_it->first 
                         << " undecodable.";
            continue;
        }

        next_decodable_frame = frame_it;

        break;
    }

    return next_decodable_frame;
}

int64_t FrameBuffer::FindNextFrame(int64_t now_ms) {
    // Waiting time before decoded.
    int64_t max_wait_ms = latest_render_time_ms_ - now_ms;
    int64_t wait_ms = max_wait_ms;
    frames_to_decode_.clear();

    for (auto frame_it = frames_.begin(); 
        frame_it != frames_.end() && frame_it->first <= last_continuous_frame_id_; 
        ++frame_it) {

        // Filter the frames is uncontinuous or undecodable.
        if (!frame_it->second.continuous() || frame_it->second.num_missing_decodable > 0) {
            continue;
        }

        video::FrameToDecode* frame = frame_it->second.frame.get();

        // Filter the delta frames if the key frame is required now.
        if (keyframe_required_ && !frame->is_keyframe()) {
            continue;
        }

        auto last_decoded_frame_timestamp = decoded_frames_history_.last_decoded_frame_timestamp();
        // TODO: Consider removing this check as it may make a stream undecodable 
        // after a very long delay (multiple wrap arounds) between frames.
        if (last_decoded_frame_timestamp && 
            wrap_around_utils::AheadOf(*last_decoded_frame_timestamp, frame->timestamp())) {
            continue;
        }
        
        // TODO: Support spatial layer, and gather all remaining frames for the same super frame.

        // Found a next decodable frame.
        frames_to_decode_.push_back(frame_it);

        // Set render time if necessary.
        if (-1 == frame->render_time_ms()) {
            frame->set_render_time_ms(timing_->RenderTimeMs(frame->timestamp(), now_ms));
        }
        
        // The max time in ms to wait before decoding the current frame.
        // FIXME: Dose the later frame has a smaller waiting time?
        wait_ms = timing_->MaxTimeWaitingToDecode(frame->render_time_ms(), now_ms);

        // This will cause the frame buffer to prefer high frame rate rather
        // than high resolution in the case of the decoder not decoding fast
        // enough and the stream has multiple spatial and temporal layers.
        if (wait_ms < -kMaxAllowedFrameDelayMs) {
            continue;
        }

        // It's time to decode the found frames.
        break;
    } // end of for

    // Limits the wait time in the range: [0, max_wait_ms]
    wait_ms = std::min<int64_t>(wait_ms, max_wait_ms);
    wait_ms = std::max<int64_t>(wait_ms, 0);

    return wait_ms;
}

video::FrameToDecode* FrameBuffer::GetNextFrame() {
    if (frames_to_decode_.empty()) {
        return nullptr;
    }
    int64_t now_ms = clock_->now_ms();
    // Consider all the frames to decode at the same time as a batch.
    size_t batch_frame_size = 0;
    bool batch_delayed_by_retransmission = false;
    video::FrameToDecode* first_frame = frames_to_decode_[0]->second.frame.get();
    int64_t render_time_ms = first_frame->render_time_ms();
    int64_t batch_received_time_ms = first_frame->received_time_ms();
    // Gracefully handle invalid RTP timestamps and render time.
    if (!ValidRenderTiming(*first_frame, now_ms)) {
        jitter_estimator_.Reset();
        timing_->Reset();
        render_time_ms = timing_->RenderTimeMs(first_frame->timestamp(), now_ms);
    }

    for (FrameMap::iterator& frame_it : frames_to_decode_) {
        FrameToDecode* frame_to_decode = frame_it->second.frame.release();
        frame_to_decode->set_render_time_ms(render_time_ms);

        batch_delayed_by_retransmission |= frame_to_decode->delayed_by_retransmission();
        batch_received_time_ms = std::max(batch_received_time_ms, frame_to_decode->received_time_ms());
        batch_frame_size += frame_to_decode->size();

        // Propagate the decodability to the dependent frames of this frame.
        PropagateDecodability(frame_it->second);

        decoded_frames_history_.InsertFrame(frame_it->first, frame_to_decode->timestamp());

        // TODO: Tragger state callback with dropped frames.
        // size_t dropped_frames = NumUndecodedFrames(frames_.begin(), frame_it)

        // Remove this decoded frame and all undecodable frames before it.
        frames_.erase(frames_.begin(), ++frame_it);
    }

    // No nack has happened during the transport of the current GOP.
    if (!batch_delayed_by_retransmission) {
        // Calculate the delay of the current GOP from the previous GOP.
        auto [frame_delay, success] = inter_frame_delay_.CalculateDelay(first_frame->timestamp() /* GOP send timestamp */, batch_received_time_ms);
        if (success) {
            assert(frame_delay >= 0);
            jitter_estimator_.UpdateEstimate(frame_delay, batch_frame_size);
        }

        // `rtt_mult` will be 1 if protection mode is NACK only.
        float rtt_mult = protection_mode_ == ProtectionMode::NACK_FEC ? 0 : 1;
        std::optional<float> rtt_mult_add_cap_ms = std::nullopt;
        // TODO: Enable RttMultExperiment if necessary.

        timing_->set_jitter_delay_ms(jitter_estimator_.GetJitterEstimate(rtt_mult, rtt_mult_add_cap_ms));
        timing_->UpdateCurrentDelay(render_time_ms, now_ms);
    } else {
        if (add_rtt_to_playout_delay_) {
            jitter_estimator_.FrameNacked();
        }
    }

    // TODO: Update jitter delay and timing frame info.

    // TODO: Return next frames to decode

    return nullptr;
}

bool FrameBuffer::ValidRenderTiming(const video::FrameToDecode& frame, int64_t now_ms) {
    int64_t render_time_ms = frame.render_time_ms();
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

void FrameBuffer::PropagateDecodability(const FrameInfo& frame_info) {
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
}

} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc