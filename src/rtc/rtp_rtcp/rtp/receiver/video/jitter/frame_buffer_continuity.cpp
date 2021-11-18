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

// Max number of frames the buffer will hold.
constexpr size_t kMaxFramesBuffered = 800;

} // namespace

std::pair<int64_t, bool> FrameBuffer::InsertFrame(video::FrameToDecode frame) {
    return task_queue_->Sync<std::pair<int64_t, bool>>([this, frame=std::move(frame)](){
        return InsertFrameIntrenal(std::move(frame));
    });
}

// Private methods
std::pair<int64_t, bool> FrameBuffer::InsertFrameIntrenal(video::FrameToDecode frame) {
    assert(task_queue_->is_in_current_queue());
    int64_t last_continuous_frame_id = last_continuous_frame_id_.value_or(-1);

    if (!ValidReferences(frame)) {
        PLOG_WARNING << "Frame " << frame.id()
                     << " has invaild frame reference, dropping it.";
        return {last_continuous_frame_id, false};
    }

    if (frame_infos_.size() >= kMaxFramesBuffered) {
        if (frame.is_keyframe()) {
            PLOG_WARNING << "Inserting keyframe " << frame.id()
                         << " but the buffer is full, clearing buffer and inserting the frame.";
            Clear();
        } else {
            return {last_continuous_frame_id, false};
        }
    }

    // The picture id (the last packet sequence number for H.264) of the last decoded frame.
    auto last_decoded_frame_id = decoded_frames_history_.last_decoded_frame_id();
    auto last_decoded_frame_timestamp = decoded_frames_history_.last_decoded_frame_timestamp();
    // Test if this frame has a earlier frame id than the last decoded frame, this can happen
    // when the frame id is out of order or wrapped around.
    if (last_decoded_frame_id && frame.id() <= *last_decoded_frame_id) {
        // This frame has a newer timestamp but an earlier frame id, this can happen
        // due to some encoder reconfiguration or picture id wrapped around.
        if (wrap_around_utils::AheadOf(frame.timestamp(), *last_decoded_frame_timestamp) 
            && frame.is_keyframe()) {
            // In this case, we assume there has been a jump in the frame id due 
            // to some encoder reconfiguration or some other reason. Even though  
            // this is not according to spec we can still continue to decode from 
            // this frame if it is a keyframe.
            PLOG_WARNING << "A jump in frame is was detected, clearing buffer.";
            // Clear and continue to decode (start from this frame).
            Clear();
            last_continuous_frame_id = -1;
        } else {
            // The frame is out of order, and it's not a keyframe, droping it.
            PLOG_WARNING << "Frame " << frame.id() << " inserted after frame "
                         << *last_decoded_frame_id
                         << " was handed off for decoding, dropping frame.";
            return {last_continuous_frame_id, false};
        }
    }

    // Test if inserting this frame would cause the order of the frame to become
    // ambigous (covering more than half the interval of 2^16). This can happen
    // when the frame id make large jumps mid stream.
    // FIXME: I don't think this can happen, since frame id has been unwrapped already.
    if (!frame_infos_.empty() && 
        frame.id() < frame_infos_.begin()->first &&
        frame.id() > frame_infos_.rbegin()->first) {
        PLOG_WARNING << "A jump in frame id was detected, clearing buffer.";
        // Clear and continue to decode (start from this frame).
        Clear();
        last_continuous_frame_id = -1;
    }

    auto [frame_it, success] = EmplaceFrameInfo(std::move(frame));

    // Frame has inserted already, dropping it.
    if (!success) {
        return {last_continuous_frame_id, false};
    }

    auto& frame_info = frame_it->second;

    // If all packets of this frame was not be retransmited, 
    // it can be used to calculate delay in Timing.
    if (!frame.delayed_by_retransmission()) {
        decode_queue_->Async([this, timestamp = frame_info.frame->timestamp(), received_time_ms=frame_info.frame->received_time_ms()](){
            timing_->IncomingTimestamp(timestamp, received_time_ms);
        });
    }

    if (auto observer = stats_observer_.lock()) {
        observer->OnCompleteFrame(frame_info.frame->is_keyframe(), frame_info.frame->size());
    }

    // The incoming frame is continuous and try to find all the decodable frames.
    if (frame_info.continuous()) {
        // Propaget continuity
        PropagateContinuity(frame_info);
        // Update the last continuous frame id with this frame id.
        last_continuous_frame_id = *last_continuous_frame_id_;
        // Try to find the decodable frames.
        FindNextDecodableFrames();
    }

    return {last_continuous_frame_id, true};
}

bool FrameBuffer::ValidReferences(const video::FrameToDecode& frame) {
    assert(task_queue_->is_in_current_queue());
    if (frame.frame_type() == VideoFrameType::KEY) {
        // Key frame has no reference.
        return frame.NumReferences() == 0;
    } else {
        bool has_invalid_ref = false;
        frame.ForEachReference([&has_invalid_ref, &frame](int64_t ref_picture_id, bool* stoped) {
            // The referred frame of this frame is behind itself, e.g the B frame.
            if (ref_picture_id >= frame.id()) {
                has_invalid_ref = true;
                *stoped = true;
            }
        });
        return !has_invalid_ref;
    }
}

std::pair<FrameBuffer::FrameInfoMap::iterator, bool> FrameBuffer::EmplaceFrameInfo(video::FrameToDecode frame) {
    assert(task_queue_->is_in_current_queue());
    int64_t new_frame_id = frame.id();
    FrameInfoMap::iterator frame_it = frame_infos_.find(new_frame_id);
    // Frame has inserted already, dropping it.
    if (frame_it != frame_infos_.end()) {
        PLOG_WARNING << "Frame with id=" << new_frame_id
                     << " already inserted, dropping it.";
        return {frame_it, false};
    }

    auto last_decoded_frame_id = decoded_frames_history_.last_decoded_frame_id();
    // The incoming frame is undecodable since the frame ahead of it was decoded.
    if (last_decoded_frame_id && *last_decoded_frame_id >= new_frame_id) {
        return {frame_it, false};;
    }

    struct Dependency {
        int64_t frame_id;
        bool continuous;
    };
    std::vector<Dependency> not_yet_fulfilled_referred_frames;
    // Indicates the incoming frame is decodable.
    bool is_frame_decodable = true;
    // Find all referred frames of this frame that have not yet been fulfilled.
    frame.ForEachReference([&](int64_t ref_frame_id, bool* stoped) {
        // Dose frame depend on a frame earlier than the last decoded frame?
        if (last_decoded_frame_id && ref_frame_id <= *last_decoded_frame_id) {
            // Was that referred frame decoded? If not, this frame will never become decodable.
            if (!decoded_frames_history_.WasDecoded(ref_frame_id)) {
                int64_t now_ms = clock_->now_ms();
                if (last_log_non_decoded_ms_ + kLogNonDecodedIntervalMs < now_ms) {
                    PLOG_WARNING << "Frame with id=" << new_frame_id
                                 << " depends on a non-decoded frame more previous than the"
                                 << " last decoded frame, dropping frame.";
                    last_log_non_decoded_ms_ = now_ms;
                }
                is_frame_decodable = false;
                *stoped = true;
            } else {
                // The referred frame was decoded.
            }
        // The referred frame is not be decoded yet.
        } else {
            // Check if the referred frame is continuous.
            auto ref_frame_it = frame_infos_.find(ref_frame_id);
            bool ref_continuous = ref_frame_it != frame_infos_.end() &&
                                  ref_frame_it->second.continuous();
            not_yet_fulfilled_referred_frames.push_back({ref_frame_id, ref_continuous});
        }
    });

    // This frame will never become decodable since
    // its referred frame was non-decodable.
    if (!is_frame_decodable) {
        return {frame_it, false};
    }

    frame_it = frame_infos_.emplace(new_frame_id, FrameInfo()).first;
    auto& frame_info = frame_it->second;
    frame_info.frame.emplace(std::move(frame));

    // The `num_missing_decodable` is the same as `num_missing_continuous` so far.
    frame_info.num_missing_continuous = not_yet_fulfilled_referred_frames.size();
    frame_info.num_missing_decodable = not_yet_fulfilled_referred_frames.size();

    // Update the dependent frame list of all the referred frames of this frame.
    for (auto& ref_frame : not_yet_fulfilled_referred_frames) {
        if (ref_frame.continuous) {
            --frame_info.num_missing_continuous;
        }
        auto ref_frame_it = frame_infos_.find(ref_frame.frame_id);
        if (ref_frame_it != frame_infos_.end()) {
            // The referred frame of this frame is not continuous for now,
            // so we keep a dependent list (as a reverse link) to propagate 
            // continuity when the referred frame becomes continuous later.
            ref_frame_it->second.dependent_frames.push_back(frame_info.frame_id());
        }
    }
    return {frame_it, true};
}

void FrameBuffer::PropagateContinuity(const FrameInfo& frame_info) {
    assert(task_queue_->is_in_current_queue());
    assert(frame_info.continuous());

    std::queue<const FrameInfo*> continuous_frames;
    continuous_frames.push(&frame_info);

    // A simple BFS to traverse continuous frames.
    while (!continuous_frames.empty()) {
        auto frame_info = continuous_frames.front();
        continuous_frames.pop();
       
        // Update the last continuous frame id with the newest frame id.
        if (!last_continuous_frame_id_ || *last_continuous_frame_id_ < frame_info->frame_id()) {
            last_continuous_frame_id_ = frame_info->frame_id();
        }

        // Loop through all dependent frames, and if that frame no longer has
        // any unfulfiied dependencies then that frame is continuous as well.
        for (int64_t dep_frame_id : frame_info->dependent_frames) {
            auto it = frame_infos_.find(dep_frame_id);
            // Test if the dependent frame is still in the buffer.
            if (it != frame_infos_.end()) {
                auto& dep_frame = it->second;
                --dep_frame.num_missing_continuous;
                // Test if the dependent frame becomes continuous so far.
                if (dep_frame.continuous()) {
                    // Push this dependent frame to `continuous_frames` and
                    // we will traverse it's dependent frames next (BFS).
                    continuous_frames.push(&dep_frame);
                }
            }
        }
    } // end of while
}

} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc