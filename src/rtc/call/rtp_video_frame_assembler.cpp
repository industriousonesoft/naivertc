#include "rtc/call/rtp_video_frame_assembler.hpp"

#include <plog/Log.h>

namespace naivertc {

constexpr size_t kMaxMissingPacketCount = 1000;

RtpVideoFrameAssembler::Packet::Packet(RtpVideoHeader video_header, 
                                       uint16_t seq_num, 
                                       uint8_t payload_type, 
                                       uint32_t timestamp, 
                                       bool marker_bit) 
    : video_header(std::move(video_header)),
      seq_num(seq_num),
      payload_type(payload_type),
      timestamp(timestamp),
      marker_bit(marker_bit) {}


RtpVideoFrameAssembler::RtpVideoFrameAssembler(size_t initial_buffer_size, size_t max_buffer_size) 
    : max_packet_buffer_size_(max_buffer_size),
      packet_buffer_(initial_buffer_size),
      first_seq_num_(0),
      first_packet_received_(false),
      is_cleared_to_first_seq_num_(false) {}

RtpVideoFrameAssembler::~RtpVideoFrameAssembler() {
    Clear();
}

void RtpVideoFrameAssembler::Insert(std::unique_ptr<Packet> packet) {
    uint16_t seq_num = packet->seq_num;
    size_t index = seq_num % packet_buffer_.size();

    if (!first_packet_received_) {
        first_seq_num_ = seq_num;
        first_packet_received_ = true;
    // `seq_num` is newer than `first_seq_num_`
    }else if (seq_num_utils::AheadOf(first_seq_num_, seq_num)) {
        // If we have explicitly cleared past this packet then it's old,
        // don't insert it, just ignore it silently.
        if (is_cleared_to_first_seq_num_) {
            return;
        }
        first_seq_num_ = seq_num;
    }

    // Different sequence number may result a same index.
    if (packet_buffer_[index] != nullptr) {
        // Duplicate packet, ignoring it.
        if (packet_buffer_[index]->seq_num == packet->seq_num) {
            return;
        }
        // Try to expand the packet buffer for new packet
        if (!ExpandPacketBufferIfNecessary(seq_num)) {
            PLOG_WARNING << "Clear packet buffer and request key frame.";
            Clear();
            return;
        }
        // New index after packet buffer expanded.
        index = seq_num % packet_buffer_.size();
    }

    // Every new packet is uncontinuous before assembled.
    packet->continuous = false;
    packet_buffer_[index] = std::move(packet);

    UpdateMissingPackets(seq_num, kMaxMissingPacketCount);

    curr_seq_num_ = seq_num;
}

void RtpVideoFrameAssembler::Clear() {
    for (auto& packet : packet_buffer_) {
        packet.reset();
    }
    first_packet_received_ = false;
    is_cleared_to_first_seq_num_ = false;
    newest_inserted_seq_num_.reset();
    missing_packets_.clear();
}

void RtpVideoFrameAssembler::Assemble() {
    uint16_t seq_num = curr_seq_num_;
    for (size_t i = 0; i < packet_buffer_.size(); i++) {
        // The current sequence number is not continuous with previous one.
        if (!IsContinuous(seq_num)) {
            break;
        }
        size_t index = seq_num % packet_buffer_.size();
        auto& curr_packet = packet_buffer_[index];
        curr_packet->continuous = true;
        // If all packets of the frame is continuous, assmbling them as a frame.
        if (curr_packet->is_last_packet_in_frame) {

        }

        ++seq_num;
    }
}

// Private methods
// To check the current sequence number is continuous with the previous one.
bool RtpVideoFrameAssembler::IsContinuous(uint16_t seq_num) {
    size_t index = seq_num % packet_buffer_.size();
    const auto& curr_packet = packet_buffer_[index];

    // Current packet is not arrived yet,
    // or the urrent packet is not belong to `seq_num`,
    // so it's not continuous.
    if (curr_packet == nullptr || curr_packet->seq_num != seq_num) {
        return false;
    }
    
    // No packets are ahead of the current packet, so it is continuous.
    if (curr_packet->is_first_packet_in_frame) {
        return true;
    }else {
        size_t prev_index = index > 0 ? index - 1 : packet_buffer_.size() - 1;
        const auto& prev_packet = packet_buffer_[prev_index];

        // Previous packet is not arrived yet,
        // or the previous sequence number is not continuous with the current one,
        // so it's not continuous.
        if (prev_packet == nullptr || prev_packet->seq_num + 1 != seq_num) {
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

void RtpVideoFrameAssembler::UpdateMissingPackets(uint16_t seq_num, size_t window_size) {
    if (!newest_inserted_seq_num_) {
        newest_inserted_seq_num_ = seq_num;
    }
    // There is a jump between `newest_inserted_seq_num_` and `seq_num`.
    if (seq_num_utils::AheadOf(seq_num, newest_inserted_seq_num_.value())) {
        // Erase the obsolete packets
        uint16_t old_seq_num = seq_num - window_size;
        auto erase_to = missing_packets_.lower_bound(old_seq_num);
        missing_packets_.erase(missing_packets_.begin(), erase_to);

        // Guard against inserting a large amout of missing packets
        // if there is a jump in the sequence number.
        if (seq_num_utils::AheadOf(old_seq_num, newest_inserted_seq_num_.value())) {
            newest_inserted_seq_num_ = old_seq_num;
        }

        // Inserting missing packets from `newest_inserted_seq_num_` to `seq_num`,
        // and resulting `newest_inserted_seq_num_` = `seq_num`.
        while (seq_num_utils::AheadOf(seq_num, ++*newest_inserted_seq_num_)) {
            missing_packets_.insert(*newest_inserted_seq_num_);
        }
    }else {
        missing_packets_.erase(seq_num);
    }
}

bool RtpVideoFrameAssembler::ExpandPacketBufferIfNecessary(uint16_t seq_num) {
    // No conflict
    bool is_conflicted = packet_buffer_[seq_num % packet_buffer_.size()] != nullptr;
    if (!is_conflicted) {
        return true;
    }
    if (packet_buffer_.size() == max_packet_buffer_size_) {
        PLOG_WARNING << "Failed to expand packet buffer as it is already at max size=" 
                     << max_packet_buffer_size_;
        return false;
    }
    bool is_overflowed = false;
    size_t new_size = std::min(max_packet_buffer_size_, 2 * packet_buffer_.size());
    while(!is_overflowed && new_size <= max_packet_buffer_size_) {
        is_conflicted = false;
        for (auto& packet : packet_buffer_) {
            // Check if We need to expand buffer
            if (packet && (packet->seq_num % new_size == seq_num)) {
                if (new_size < max_packet_buffer_size_) {
                    new_size = std::min(max_packet_buffer_size_, 2 * new_size);
                // No more space to expand
                }else {
                    is_overflowed = true;
                }
                is_conflicted = true;
                break;
            }
        }
    }
    if (!is_conflicted) {
        ExpandPacketBuffer(new_size);
    }
    return !is_conflicted;
}

void RtpVideoFrameAssembler::ExpandPacketBuffer(size_t new_size) {
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
    
} // namespace naivertc
