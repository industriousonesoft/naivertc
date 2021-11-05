#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/packet_buffer.hpp"
#include "rtc/media/video/codecs/h264/common.hpp"

#include <plog/Log.h>

#include <variant>

namespace naivertc {
namespace rtp {
namespace video {
namespace jitter {

constexpr size_t kMaxMissingPacketCount = 1000;

// Packet
PacketBuffer::Packet::Packet(RtpVideoHeader video_header, 
                             RtpVideoCodecHeader video_codec_header,
                             uint16_t seq_num, 
                             uint32_t timestamp) 
    : video_header(std::move(video_header)),
      video_codec_header(std::move(video_codec_header)),
      seq_num(seq_num),
      timestamp(timestamp) {}

// PacketBuffer
PacketBuffer::PacketBuffer(size_t initial_buffer_size, 
                           size_t max_buffer_size) 
    : max_packet_buffer_size_(max_buffer_size),
      packet_buffer_(initial_buffer_size),
      first_seq_num_(0),
      first_packet_received_(false),
      is_cleared_to_first_seq_num_(false),
      sps_pps_idr_is_h264_keyframe_(true) {}

PacketBuffer::~PacketBuffer() {
    Clear();
}

PacketBuffer::InsertResult PacketBuffer::InsertPacket(std::unique_ptr<Packet> packet) {
    InsertResult ret;
    uint16_t seq_num = packet->seq_num;
    size_t index = seq_num % packet_buffer_.size();

    if (!first_packet_received_) {
        first_seq_num_ = seq_num;
        first_packet_received_ = true;
    // `seq_num` is newer than `first_seq_num_`
    } else if (wrap_around_utils::AheadOf(first_seq_num_, seq_num)) {
        // If we have explicitly cleared past this packet then it's old,
        // don't insert it, just ignore it silently.
        if (is_cleared_to_first_seq_num_) {
            return ret;
        }
        first_seq_num_ = seq_num;
    }

    // Different sequence number may result a same index.
    if (packet_buffer_[index] != nullptr) {
        // Duplicate packet, ignoring it.
        if (packet_buffer_[index]->seq_num == packet->seq_num) {
            return ret;
        }
        // Try to expand the packet buffer for new packet
        if (!ExpandPacketBufferIfNecessary(seq_num)) {
            PLOG_WARNING << "Clear packet buffer and request key frame.";
            Clear();
            ret.keyframe_requested = true;
            return ret;
        }
        // New index after packet buffer expanded.
        index = seq_num % packet_buffer_.size();
    }
    
    // Every new packet is uncontinuous before assembled.
    packet->continuous = false;
    packet_buffer_[index] = std::move(packet);

    UpdateMissingPackets(seq_num, kMaxMissingPacketCount);

    // Try to assemble frames
    ret.assembled_frames = TryToAssembleFrames(seq_num);

    return ret;
}

PacketBuffer::InsertResult PacketBuffer::InsertPadding(uint16_t seq_num) {
    InsertResult ret;
    UpdateMissingPackets(seq_num, kMaxMissingPacketCount);
    ret.assembled_frames = TryToAssembleFrames(seq_num + 1);
    return ret;
}

void PacketBuffer::Clear() {
    for (auto& packet : packet_buffer_) {
        packet.reset();
    }
    first_packet_received_ = false;
    is_cleared_to_first_seq_num_ = false;
    newest_inserted_seq_num_.reset();
    missing_packets_.clear();
}

void PacketBuffer::ClearTo(uint16_t seq_num) {
    // We have already cleared past this sequence number, no need to do anything.
    if (is_cleared_to_first_seq_num_ && wrap_around_utils::AheadOf<uint16_t>(first_seq_num_, seq_num)) {
        return;
    }

    // If the packet buffer was cleared between a frame was created and returned.
    if (!first_packet_received_) {
        return;
    }

    // Avoid iterating over the buffer more than once by capping the number of
    // iterations to the |size_| of the buffer.
    ++seq_num;
    size_t diff = ForwardDiff<uint16_t>(first_seq_num_, seq_num);
    size_t iterations = std::min(diff, packet_buffer_.size());
    for (size_t i = 0; i < iterations; ++i) {
        auto& stored = packet_buffer_[first_seq_num_ % packet_buffer_.size()];
        if (stored != nullptr && wrap_around_utils::AheadOf<uint16_t>(seq_num, stored->seq_num)) {
            stored = nullptr;
        }
        ++first_seq_num_;
    }

    // If |diff| is larger than |iterations| it means that we don't increment
    // |first_seq_num_| until we reach |seq_num|, so we set it here.
    first_seq_num_ = seq_num;

    is_cleared_to_first_seq_num_ = true;
    auto clear_to_it = missing_packets_.upper_bound(seq_num);
    if (clear_to_it != missing_packets_.begin()) {
        --clear_to_it;
        missing_packets_.erase(missing_packets_.begin(), clear_to_it);
    }
}

// Private methods
PacketBuffer::AssembledFrames PacketBuffer::TryToAssembleFrames(uint16_t seq_num) {
    AssembledFrames assembled_frames;
    for (size_t i = 0; i < packet_buffer_.size(); i++) {
        // The current sequence number is not continuous with previous one.
        if (!IsContinuous(seq_num)) {
            break;
        }
        size_t index = seq_num % packet_buffer_.size();
        Packet* curr_packet = packet_buffer_[index].get();
        curr_packet->continuous = true;
        // If all packets of the frame is continuous, try to assmble them as a frame.
        if (packet_buffer_[index]->video_header.is_last_packet_in_frame) {
            uint16_t seq_num_start = seq_num;
            size_t tested_packets = 0;
            int index_in_frame = index;
            int64_t frame_timestamp = curr_packet->timestamp;
            size_t frame_size = 0;
            
            // Identify H264 keyframes by means of SPS, PPS, and IDR.
            bool is_h264 = curr_packet->video_header.codec_type == ::naivertc::VideoCodecType::H264;
            bool is_h264_keyframe = false;
            bool has_h264_sps_in_frame = false;
            bool has_h264_pps_in_frame = false;
            bool has_h264_idr_in_frame = false;
            int idr_width = -1;
            int idr_height = -1;
        
            // Find all the packets belonging to a completed frame.
            while (true) {
                ++tested_packets;

                const auto& video_header = curr_packet->video_header;
                frame_size += curr_packet->video_payload.size();

                // `is_first_packet_in_frame` flag not works for H264
                if (!is_h264 && video_header.is_first_packet_in_frame) {
                    break;
                }

                // Check if there has SPS, PPS or IDR in the packet of the assembling frame.
                if (is_h264) {
                    // TODO: Using std::get_if instead.
                    const auto& h264_header = std::get<h264::PacketizationInfo>(curr_packet->video_codec_header);

                    if (h264_header.nalus.size() >= h264::kMaxNaluNumPerPacket) {
                        return assembled_frames;
                    }

                    if (h264_header.has_sps) {
                        has_h264_sps_in_frame = true;
                    }
                    if (h264_header.has_pps) {
                        has_h264_pps_in_frame = true;
                    }
                    if (h264_header.has_idr) {
                        has_h264_idr_in_frame = true;
                    }
                    // The conditions that determines if the H.264-IDR frame is a key frame.
                    if ((sps_pps_idr_is_h264_keyframe_ && has_h264_sps_in_frame && has_h264_pps_in_frame && has_h264_idr_in_frame) ||
                        (!sps_pps_idr_is_h264_keyframe_ && has_h264_idr_in_frame)) {
                        is_h264_keyframe = true;
                        if (video_header.frame_width > 0 && video_header.frame_height > 0) {
                            idr_width = video_header.frame_width;
                            idr_height = video_header.frame_height;
                        }
                    }
                }

                if (tested_packets == packet_buffer_.size()) {
                    break;
                }

                // Backwards to previous packet 
                index_in_frame = index_in_frame > 0 ? index_in_frame - 1 : packet_buffer_.size() - 1;

                curr_packet = packet_buffer_[index_in_frame].get();

                // In the case of H264 we don't have a frame_begin bit (yes,
                // |frame_begin| might be set to true but that is a lie). So instead
                // we traverese backwards as long as we have a previous packet and
                // the timestamp of that packet is the same as this one. This may cause
                // the PacketBuffer to hand out incomplete frames.
                // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=7106
                if (is_h264 && (curr_packet == nullptr || curr_packet->timestamp != frame_timestamp)) {
                    break;
                }

                // The previous packet exists.
                --seq_num_start;

            } // end of while

            if (is_h264) {
                 // Warn if this is an unsafe frame.
                if (has_h264_idr_in_frame && (!has_h264_sps_in_frame || !has_h264_pps_in_frame)) {
                PLOG_WARNING
                    << "Received H.264-IDR frame (SPS: "
                    << has_h264_sps_in_frame << ", PPS: " << has_h264_pps_in_frame << "). Treating as "
                    << (sps_pps_idr_is_h264_keyframe_ ? "delta" : "key")
                    << " frame since SpsPpsIdrIsH264Keyframe is "
                    << (sps_pps_idr_is_h264_keyframe_ ? "enabled." : "disabled");
                }

                // Now that we have decided whether to treat this frame as a key frame
                // or delta frame in the frame buffer, we update the `frame_type` of the first 
                // packet in the frame that determines if the frame is a key frame or delta frame.
                const size_t first_packet_index = seq_num_start % packet_buffer_.size();
                if (is_h264_keyframe) {
                    packet_buffer_[first_packet_index]->video_header.frame_type = VideoFrameType::KEY;
                    if (idr_width > 0 && idr_height > 0) {
                        // IDR frame was finalized and we have the correct resolution for
                        // IDR; update first packet to have same resolution as IDR.
                        packet_buffer_[first_packet_index]->video_header.frame_width = idr_width;
                        packet_buffer_[first_packet_index]->video_header.frame_height = idr_height;
                    }
                } else {
                    packet_buffer_[first_packet_index]->video_header.frame_type = VideoFrameType::DELTA;
                }

                // If this is not a keyframe, make sure there are no gaps in the packet
                // sequence numbers up until this point.
                if (!is_h264_keyframe && missing_packets_.upper_bound(seq_num_start) != missing_packets_.begin()) {
                    return assembled_frames;
                }
            }
            const uint16_t end_seq_num = seq_num + 1;
            uint16_t num_packets = end_seq_num - seq_num_start;
            auto frame = std::make_unique<Frame>();
            frame->num_packets = num_packets;
            frame->bitstream.Resize(frame_size);
            uint8_t* write_at = frame->bitstream.data();
            // NOTE: Using `!=` not `<` to make sure the wrapped around sequence number works.
            // e.g.: seq_num_start=0xffff, end_seq_num=1
            for (uint16_t i = seq_num_start; i != end_seq_num; ++i) {
                std::unique_ptr<Packet>& packet = packet_buffer_[i % packet_buffer_.size()];
                assert(packet != nullptr);
                assert(i == packet->seq_num);
                // The first packet in frame
                if (i == seq_num_start) {
                    frame->frame_width = packet->video_header.frame_width;
                    frame->frame_height = packet->video_header.frame_height;
                    frame->codec_type = packet->video_header.codec_type;
                    frame->frame_type = packet->video_header.frame_type;
                    frame->seq_num_start = packet->seq_num;
                    frame->timestamp = packet->timestamp;
                    frame->times_nacked = packet->times_nacked;
                    frame->min_received_time_ms = packet->received_time_ms;
                    frame->max_received_time_ms = packet->received_time_ms;
                }
                // The last packet in frame
                if (i == seq_num) {
                    frame->seq_num_end = packet->seq_num;
                }

                frame->times_nacked = std::max(frame->times_nacked, frame->times_nacked);
                frame->min_received_time_ms = std::min(frame->min_received_time_ms, packet->received_time_ms);
                frame->max_received_time_ms = std::max(frame->max_received_time_ms, packet->received_time_ms);

                // Append payload data
                memcpy(write_at, packet->video_payload.data(), packet->video_payload.size());
                write_at += packet->video_payload.size();
                packet.reset();
            }

            assert(frame_size == frame->bitstream.size());

            assembled_frames.push_back(std::move(frame));

            missing_packets_.erase(missing_packets_.begin(), missing_packets_.upper_bound(seq_num));
        }
        ++seq_num;
    }
    return assembled_frames;
}

// To check the current sequence number is continuous with the previous one.
bool PacketBuffer::IsContinuous(uint16_t seq_num) {
    size_t index = seq_num % packet_buffer_.size();
    const auto& curr_packet = packet_buffer_[index];

    // Current packet is not arrived yet,
    if (curr_packet == nullptr) {
        return false;
    }

    // The urrent packet is not belong to `seq_num`,
    // so it's not continuous.
    if (curr_packet->seq_num != seq_num) {
        return false;
    }
    
    // No packets are ahead of the current packet, so it is continuous.
    if (curr_packet->video_header.is_first_packet_in_frame) {
        return true;
    } else {
        size_t prev_index = index > 0 ? index - 1 : packet_buffer_.size() - 1;
        const auto& prev_packet = packet_buffer_[prev_index];

        // Previous packet is not arrived yet,
        if (prev_packet == nullptr) {
            return false;
        }
        // The previous sequence number is not continuous with the current one,
        // so it's not continuous.
        if (prev_packet->seq_num != static_cast<uint16_t>(curr_packet->seq_num - 1)) {
            return false;
        }
        // All packets in a frame must have the same timestamp, 
        // otherwise, it means not continuous.
        if (prev_packet->timestamp != curr_packet->timestamp) {
            return false;
        }

        // So far, the current packet is continuous with the previous one.
        // The result depends on the previous packet is continuous or not.
        return prev_packet->continuous;
    }
}

void PacketBuffer::UpdateMissingPackets(uint16_t seq_num, size_t window_size) {
    if (!newest_inserted_seq_num_) {
        newest_inserted_seq_num_ = seq_num;
    }
    // There is a jump between `newest_inserted_seq_num_` and `seq_num`.
    if (wrap_around_utils::AheadOf(seq_num, newest_inserted_seq_num_.value())) {
        // Erase the obsolete packets
        uint16_t old_seq_num = seq_num - window_size;
        auto erase_to = missing_packets_.lower_bound(old_seq_num);
        missing_packets_.erase(missing_packets_.begin(), erase_to);

        // Guard against inserting a large amout of missing packets
        // if there is a jump in the sequence number.
        if (wrap_around_utils::AheadOf(old_seq_num, newest_inserted_seq_num_.value())) {
            newest_inserted_seq_num_ = old_seq_num;
        }

        // Inserting missing packets from `newest_inserted_seq_num_` to `seq_num`,
        // and resulting `newest_inserted_seq_num_` = `seq_num`.
        while (wrap_around_utils::AheadOf(seq_num, ++*newest_inserted_seq_num_)) {
            missing_packets_.insert(*newest_inserted_seq_num_);
        }
    } else {
        missing_packets_.erase(seq_num);
    }
}

bool PacketBuffer::ExpandPacketBufferIfNecessary(uint16_t seq_num) {
    // No conflict
    if (packet_buffer_[seq_num % packet_buffer_.size()] == nullptr) {
        return true;
    }
    if (packet_buffer_.size() == max_packet_buffer_size_) {
        PLOG_WARNING << "Failed to expand packet buffer as it is already at max size=" 
                     << max_packet_buffer_size_;
        return false;
    }
    bool no_more_space_to_expand = false;
    size_t new_size = std::min(max_packet_buffer_size_, 2 * packet_buffer_.size());
    while(!no_more_space_to_expand && new_size <= max_packet_buffer_size_) {
        auto it = packet_buffer_.begin();
        for (; it != packet_buffer_.end(); ++it) {
            auto& packet = *it;
            // Check if We need to expand buffer
            if (packet && (packet->seq_num % new_size == seq_num)) {
                if (new_size < max_packet_buffer_size_) {
                    new_size = std::min(max_packet_buffer_size_, 2 * new_size);
                    no_more_space_to_expand = false;
                // No more space to expand
                } else {
                    no_more_space_to_expand = true;
                }
                break;
            }
        }
        // Conflict fixed.
        if (it == packet_buffer_.end()) {
            break;
        }
    }
    if (!no_more_space_to_expand) {
        ExpandPacketBuffer(new_size);
    }
    return !no_more_space_to_expand;
}

void PacketBuffer::ExpandPacketBuffer(size_t new_size) {
    if (packet_buffer_.size() == max_packet_buffer_size_) {
        PLOG_WARNING << "Failed to expand packet buffer as it is already at max size=" 
                     << max_packet_buffer_size_;
        return;
    }
    new_size = std::min(max_packet_buffer_size_, new_size);
    std::vector<std::unique_ptr<Packet>> new_packet_buffer(new_size);
    for (auto& packet : packet_buffer_) {
        if (packet) {
            new_packet_buffer[packet->seq_num % new_size] = std::move(packet);
        }
    }
    packet_buffer_ = std::move(new_packet_buffer);
    PLOG_INFO << "Packet buffer expanded to " << new_size;
}
    
} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc
