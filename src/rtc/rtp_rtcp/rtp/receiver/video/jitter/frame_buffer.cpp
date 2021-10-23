#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_buffer.hpp"
#include "rtc/rtp_rtcp/components/seq_num_utils.hpp"

#include <plog/Log.h>

#include <algorithm>
#include <queue>

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {
namespace {

// Max number of frames the buffer will hold.
constexpr size_t kMaxFramesBuffered = 800;
// Max number of decoded frame info that will be saved.
constexpr int kMaxFramesHistory = 1 << 13; // 8192

constexpr int64_t kLogNonDecodedIntervalMs = 5000;
    
} // namespace


FrameBuffer::FrameBuffer(std::shared_ptr<Clock> clock)
    : clock_(std::move(clock)),
      decoded_frames_history_(kMaxFramesHistory),
      last_log_non_decoded_ms_(-kLogNonDecodedIntervalMs) {}

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
            Clear();
        } else {
            return last_continuous_frame_id;
        }
    }

    // The picture id (the last packet sequence number for H.264) of the last decoded frame.
    auto last_decoded_frame_id = decoded_frames_history_.last_decoded_frame_id();
    auto last_decoded_frame_timestamp = decoded_frames_history_.last_decoded_frame_timestamp();
    // Test if this frame has a earlier frame id than the last decoded frame, this can happen
    // when the frame id is out of order or wrapped around.
    if (last_decoded_frame_id && frame->id() <= *last_decoded_frame_id) {
        // This frame has a newer timestamp but an earlier frame id, this can happen
        // due to some encoder reconfiguration or picture id wrapped around.
        if (seq_num_utils::AheadOf(frame->timestamp(), *last_decoded_frame_timestamp) 
            && frame->is_keyframe()) {
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
            PLOG_WARNING << "Frame " << frame->id() << " inserted after frame "
                         << *last_decoded_frame_id
                         << " was handed off for decoding, dropping frame.";
            return last_continuous_frame_id;
        }
    }

    // Test if inserting this frame would cause the order of the frame to become
    // ambigous (covering more than half the interval of 2^16). This can happen
    // when the frame id make large jumps mid stream.
    int64_t first_frame_id_buffered = frame_buffer_.begin()->first;
    int64_t last_frame_id_buffered = frame_buffer_.rbegin()->first;
    // FIXME: Why using `&&` not `||` here?
    if (!frame_buffer_.empty() && 
        frame->id() < first_frame_id_buffered &&
        frame->id() > last_frame_id_buffered) {
        PLOG_WARNING << "A jump in frame id was detected, clearing buffer.";
        // Clear and continue to decode (start from this frame).
        Clear();
        last_continuous_frame_id = -1;
    }

    auto& frame_info = frame_buffer_.emplace(frame->id(), FrameInfo()).first->second;
    
    // Frame has inserted already, dropping it.
    if (frame_info.frame) {
        PLOG_WARNING << "Frame " << frame->id()
                     << " already inserted, dropping it.";
        return last_continuous_frame_id;
    }

    // Update frame info with incoming frame.
    if (!UpdateFrameInfo(frame_info)) {
        return last_continuous_frame_id;
    }

    // If all packets of this frame was not be retransmited, 
    // it can be used to calculate delay in Timing.
    if (!frame->delayed_by_retransmission()) {
        // TODO: Update Timing 
    }

    // TODO: Trigger the state callback

    frame_info.frame = std::move(frame);

    // Test if this frame is continuous or not.
    if (frame_info.num_missing_continuous == 0) {
        frame_info.continuous = true;

        // Propaget continuity
        PropagateContinuity(frame_info);

        // Update the last continuous frame id with this frame id.
        last_continuous_frame_id = *last_continuous_frame_id_;
        
        // TODO: Ready to decode this frame.
    }

    return last_continuous_frame_id;
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
    decoded_frames_history_.Clear();
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

bool FrameBuffer::UpdateFrameInfo(FrameInfo& frame_info) {
    auto last_decoded_frame_id = decoded_frames_history_.last_decoded_frame_id();
    // Invalid frame to decode.
    if (last_decoded_frame_id && *last_decoded_frame_id >= frame_info.frame_id()) {
        return false;
    }

    struct Dependency {
        int64_t frame_id;
        bool continuous;
    };
    std::vector<Dependency> not_yet_fulfilled_referred_frames;
    bool is_frame_decodable = true;
    // Find all referred frames of this frame that have not yet been fulfilled.
    frame_info.frame->ForEachReference([&](int64_t ref_frame_id, bool* stoped) {
        // Dose `frame` depend on a frame earlier than the last decoded frame?
        if (last_decoded_frame_id && ref_frame_id <= *last_decoded_frame_id) {
            // Was that frame decoded? If not, this `frame` will never become decodable.
            if (!decoded_frames_history_.WasDecoded(ref_frame_id)) {
                int64_t now_ms = clock_->now_ms();
                if (last_log_non_decoded_ms_ + kLogNonDecodedIntervalMs < now_ms) {
                    PLOG_WARNING << "Frame " << frame_info.frame_id() 
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
            auto ref_frame_it = frame_buffer_.find(ref_frame_id);
            bool ref_continuous = ref_frame_it != frame_buffer_.end() &&
                                  ref_frame_it->second.continuous;
            not_yet_fulfilled_referred_frames.push_back({ref_frame_id, ref_continuous});
        }
    });

    // This frame will never become decodable since
    // its referred frame was non-decodable.
    if (!is_frame_decodable) {
        return false;
    }

    // The `num_missing_decodable` is the same as `num_missing_continuous` so far.
    frame_info.num_missing_continuous = not_yet_fulfilled_referred_frames.size();
    frame_info.num_missing_decodable = not_yet_fulfilled_referred_frames.size();

    // Update the dependent frame list of all the referred frames of this frame.
    for (auto& ref_frame : not_yet_fulfilled_referred_frames) {
        if (ref_frame.continuous) {
            --frame_info.num_missing_continuous;
        }
        // The referred frame of this frame is not continuous for now,
        // so we keep a dependent list (as a reverse link) to propagate 
        // continuity when the referred frame becomes continuous later.
        frame_buffer_[ref_frame.frame_id].dependent_frames.push_back(frame_info.frame_id());
    }

    return true;
}

void FrameBuffer::PropagateContinuity(const FrameInfo& frame_info) {
    if (!frame_info.continuous || frame_info.frame == nullptr) {
        return;
    }

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
            auto it = frame_buffer_.find(dep_frame_id);
            // Test if the dependent frame is still in the buffer.
            if (it != frame_buffer_.end()) {
                auto& dep_frame = it->second;
                --dep_frame.num_missing_continuous;
                // Test if the dependent frame becomes continuous so far.
                if (dep_frame.num_missing_continuous == 0) {
                    dep_frame.continuous = true;
                    // Push this dependent frame to `continuous_frames` and
                    // we will traverse it's dependent frames next (BFS).
                    continuous_frames.push(&dep_frame);
                }
            }
        }
    }
}
    
} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivertc