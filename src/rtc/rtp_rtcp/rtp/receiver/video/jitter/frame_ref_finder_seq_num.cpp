#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/frame_ref_finder_seq_num.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {
namespace {

constexpr size_t kMaxGopPacketAge = 10000;
constexpr size_t kMaxGopInfoAge = 100;
constexpr size_t kMaxPaddingAge = 100;
constexpr size_t kMaxStashedFrames = 100;
    
} // namespace

SeqNumFrameRefFinder::SeqNumFrameRefFinder() {}

SeqNumFrameRefFinder::~SeqNumFrameRefFinder() {}

void SeqNumFrameRefFinder::InsertFrame(std::unique_ptr<video::FrameToDecode> frame) {
    if (frame == nullptr) {
        return;
    }
    FrameDecision decision = FindRefForFrame(*(frame.get()));
    if (decision == FrameDecision::STASHED) {
        if (stashed_frames_.size() > kMaxStashedFrames) {
            stashed_frames_.pop_back();
        } 
        stashed_frames_.push_front(std::move(frame));
    } else if (decision == FrameDecision::HAND_OFF) {
        if (frame_ref_found_callback_) {
            frame_ref_found_callback_(std::move(frame));
        }
        // Retry to find reference for the stashed frames,
        // as there may has a stashed frame referring to this frame.
        RetryStashedFrames();
    }
}

void SeqNumFrameRefFinder::InsertPadding(uint16_t seq_num) {
    auto clean_padding_to = stashed_padding_.lower_bound(seq_num - kMaxPaddingAge);
    stashed_padding_.erase(stashed_padding_.begin(), clean_padding_to);
    stashed_padding_.insert(seq_num);
    UpdateGopInfo(seq_num);
    // Retry to find reference for the stashed frames,
    // as there has a stashed frame referring to 
    // this padding 'frame' (just one packet itself).
    RetryStashedFrames();
}

void SeqNumFrameRefFinder::ClearTo(uint16_t seq_num) {
    auto it = stashed_frames_.begin();
    while (it != stashed_frames_.end()) {
        // FIXME: Why we here compared the sequence num with `first_packet_seq_num` not `last_packet_seq_num`?
        if (seq_num_utils::AheadOf<uint16_t>(seq_num, (*it)->first_packet_seq_num())) {
            it = stashed_frames_.erase(it);
        } else {
            ++it;
        }
    }
}

// Private methods
SeqNumFrameRefFinder::FrameDecision SeqNumFrameRefFinder::FindRefForFrame(video::FrameToDecode& frame) {
    // We received a keyframe,
    if (frame.frame_type() == VideoFrameType::KEY) {
        gop_infos_.insert({frame.last_packet_seq_num(), {frame.last_packet_seq_num(), frame.last_packet_seq_num()}});
    }

    // We have received a frame, but not yet a keyframe,
    // stashing this frame, and try it later.
    if (gop_infos_.empty()) {
        return FrameDecision::STASHED;
    }

    // Clean up old keyframes but make sure to keep info for the last keyframe.
    auto clean_to = gop_infos_.lower_bound(frame.last_packet_seq_num() - kMaxGopInfoAge);
    for (auto it = gop_infos_.begin(); it != clean_to && gop_infos_.size() > 1;) {
        it = gop_infos_.erase(it);
    }

    // Find the last sequence number (picture id) of the last frame for the keyframe
    // that this frame indirectly references.
    auto next_gop_picture_id_it = gop_infos_.upper_bound(frame.last_packet_seq_num());
    if (next_gop_picture_id_it == gop_infos_.begin()) {
        PLOG_WARNING << "Generic frame with packet range ["
                     << frame.first_packet_seq_num() << ", "
                     << frame.last_packet_seq_num()
                     << "] has no GOP, dropping it.";
        return FrameDecision::DROPED;
    }

    auto curr_gop_picture_id_it = next_gop_picture_id_it--;
    // The frame is not continuous with the last frame in the GOP, stashing it.
    if (frame.frame_type() == VideoFrameType::DELTA && 
        frame.first_packet_seq_num() - 1 != curr_gop_picture_id_it->second.last_picture_id_with_padding_gop) {
            return FrameDecision::STASHED;
    }

    assert(seq_num_utils::AheadOrAt<uint16_t>(frame.last_packet_seq_num(), curr_gop_picture_id_it->first));

    PictureId last_picture_id_gop = curr_gop_picture_id_it->second.last_picture_id_gop;
    // the keyframe has no reference frames, but the delta frame has.
    if (frame.frame_type() == VideoFrameType::DELTA) {
        frame.AddReference(seq_num_unwrapper_.Unwrap(last_picture_id_gop));
    }

    // Using the sequence number of the last packet of frame as picture id.
    uint16_t curr_picture_id = frame.last_packet_seq_num();
    // Check if the current frame is newest in the GOP.
    if (seq_num_utils::AheadOf<uint16_t>(curr_picture_id, last_picture_id_gop)) {
        curr_gop_picture_id_it->second.last_picture_id_gop = curr_picture_id;
        curr_gop_picture_id_it->second.last_picture_id_with_padding_gop = curr_picture_id;
    }

    UpdateGopInfo(curr_picture_id);
    // Using unwrapped sequence number to make sure the frame is unique.
    frame.set_id(seq_num_unwrapper_.Unwrap(curr_picture_id));

    return FrameDecision::HAND_OFF;
}

void SeqNumFrameRefFinder::UpdateGopInfo(uint16_t seq_num) {
    auto next_gop_picture_id_it = gop_infos_.upper_bound(seq_num);
    // This padding packet belongs to a GOP than we don't track anymore.
    if (next_gop_picture_id_it == gop_infos_.begin()) {
        return;
    }

    auto curr_gop_picture_id_it = next_gop_picture_id_it--;

    // Find the next continuous sequence number
    PictureId next_picture_id_with_padding = curr_gop_picture_id_it->second.last_picture_id_with_padding_gop + 1;
    auto padding_picture_id_it = stashed_padding_.lower_bound(next_picture_id_with_padding);

    // While there still are padding packets and those padding packets are continuous,
    // then advance the `last_picture_id_with_padding_gop` and remove the stashed padding
    // packet.
    while (padding_picture_id_it != stashed_padding_.end() && *padding_picture_id_it == next_picture_id_with_padding) {
        curr_gop_picture_id_it->second.last_picture_id_with_padding_gop = next_picture_id_with_padding;
        ++next_picture_id_with_padding;
        padding_picture_id_it = stashed_padding_.erase(padding_picture_id_it);
    }

    // In the case where the stream has been continuous without any new
    // keyframes for a while, there is a risk that new frames will appear
    // to be older than the keyframe they belong to due to wrapping sequence
    // number. In order to prevent this we advance the picture id of the
    // keyframe every so often.
    if (ForwardDiff(curr_gop_picture_id_it->first, seq_num) > kMaxGopPacketAge) {
        PLOG_WARNING << "Advanced the picture id of the keyframe as having not received new keyframe for a while.";
        auto curr_gop_info = curr_gop_picture_id_it->second;
        gop_infos_.clear();
        gop_infos_[seq_num] = curr_gop_info;
    }
}

void SeqNumFrameRefFinder::RetryStashedFrames() {
    bool ref_found = false;
    do {
        ref_found = false;
        for (auto frame_it = stashed_frames_.begin(); frame_it != stashed_frames_.end();) {
            FrameDecision decision = FindRefForFrame(*(frame_it->get()));
            // Not found yet, move to next frame.
            if (decision == FrameDecision::STASHED) {
                ++frame_it;
                break;
            }else {
                if (decision == FrameDecision::HAND_OFF) {
                    ref_found = true;
                    if (frame_ref_found_callback_) {
                        frame_ref_found_callback_(std::move(*frame_it));
                    }
                }
                // Erase the frame dropped or handed off.
                frame_it = stashed_frames_.erase(frame_it);
            }
        }
    } while (ref_found);
}
    
} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivertc