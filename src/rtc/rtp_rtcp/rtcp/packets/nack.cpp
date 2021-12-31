#include "rtc/rtp_rtcp/rtcp/packets/nack.hpp"
#include "rtc/rtp_rtcp/rtcp/packets/common_header.hpp"
#include "rtc/base/memory/byte_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {

// RFC 4585, section 6.1: Feedback format.
//
// Common packet format:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P|   FMT   |       PT      |          length               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 0 |                  SSRC of packet sender                        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 4 |                  SSRC of media source                         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   :            Feedback Control Information (FCI)                 :
//   :                                                               :
//
// Generic NACK (RFC 4585).
//
// FCI:
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |            PID                |             BLP               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Nack::Nack() = default;
Nack::Nack(const Nack&) = default;
Nack::~Nack() = default;

void Nack::set_packet_ids(const uint16_t* nack_list, size_t size) {
    if (nack_list == nullptr) {
        return;
    }
    set_packet_ids(std::vector<uint16_t>(nack_list, nack_list + size));
}

void Nack::set_packet_ids(std::vector<uint16_t> nack_list) {
    packet_ids_.clear();
    packet_ids_ = std::move(nack_list);
    PackFciItems();
}

void Nack::PackFciItems() {
    if (packet_ids_.empty()) return;
    fci_items_.clear();
    auto it = packet_ids_.begin();
    const auto end = packet_ids_.end();
    while (it != end) {
        FciItem item;
        item.first_pid = *it++;
        // Bitmap specifies losses in any of the 16 packets following the packet id
        item.bitmask = 0;
        while (it != end) {
            uint16_t shift = static_cast<uint16_t>(*it - item.first_pid -1);
            if (shift <= 15) {
                item.bitmask |= (1 << shift);
                ++it;
            } else {
                break;
            }
        }
        fci_items_.push_back(item);
    }
}

void Nack::UnpackFciItems() {
    if (fci_items_.empty()) return;
    packet_ids_.clear();
    for (const auto& item : fci_items_) {
        packet_ids_.push_back(item.first_pid);
        uint16_t pid = item.first_pid + 1;
        for (uint16_t bitmask = item.bitmask; bitmask != 0; bitmask >>= 1, ++pid) {
            if (bitmask & 1) {
                packet_ids_.push_back(pid);
            }
        }
    }
}

size_t Nack::PacketSize() const {
    return kRtcpCommonHeaderSize + kCommonFeedbackSize + fci_items_.size() * kFciItemSize;
}

bool Nack::Parse(const CommonHeader& packet) {
    if (packet.type() != Nack::kPacketType) {
        PLOG_WARNING << "Incoming packet is not a Payload-specific feedback packet.";
        return false;
    }
    if (packet.feedback_message_type() != kFeedbackMessageType) {
        PLOG_WARNING << "Incoming packet is not a NACK packet.";
        return false;
    }
    if (packet.payload_size() < kCommonFeedbackSize + kFciItemSize) {
        PLOG_WARNING << "Payload size "
                     << packet.payload_size()
                     << " is too small for a NACK packet.";
        return false;
    }
    size_t fci_item_count = (packet.payload_size() - kCommonFeedbackSize) / kFciItemSize;
    Psfb::ParseCommonFeedback(packet.payload());

    const uint8_t* next_nack = packet.payload() + kCommonFeedbackSize;
    fci_items_.clear();
    fci_items_.resize(fci_item_count);
    for (size_t index = 0; index < fci_item_count; ++index) {
        fci_items_[index].first_pid = ByteReader<uint16_t>::ReadBigEndian(next_nack);
        fci_items_[index].bitmask = ByteReader<uint16_t>::ReadBigEndian(next_nack + 2);
        next_nack += kFciItemSize;
    }
    UnpackFciItems();
    return true;
}

bool Nack::PackInto(uint8_t* buffer,
                size_t* index,
                size_t max_size,
                PacketReadyCallback callback) const {
    if (fci_items_.empty()) {
        PLOG_WARNING << "No FCI item in NACK packet.";
        return false;
    }
    // RTCP common header + Payload-specific feedback fields
    constexpr size_t kNackHeaderSize = kRtcpCommonHeaderSize + kCommonFeedbackSize;
    // If nack list can't fit in packet, try to fragment.
    for (size_t fci_index = 0; fci_index < fci_items_.size();) {
        size_t bytes_left_in_buffer = max_size - *index;
        if (bytes_left_in_buffer < kNackHeaderSize + kFciItemSize) {
            if (!OnBufferFull(buffer, index, callback)) {
                return false;
            }
            continue;
        }
        // The count of fci items can be packed in this turn.
        size_t fci_item_count = std::min((bytes_left_in_buffer - kNackHeaderSize) / kFciItemSize, fci_items_.size() - fci_index);
        size_t curr_payload_size = kCommonFeedbackSize + (fci_item_count * kFciItemSize);

        // Pack current fci item as a new NACK packet
        RtcpPacket::PackCommonHeader(kFeedbackMessageType, kPacketType, curr_payload_size, buffer, index);
        Psfb::PackCommonFeedback(&buffer[*index]);
        *index += kCommonFeedbackSize;

        size_t fci_end_index = fci_index + fci_item_count;
        for (; fci_index < fci_end_index; ++fci_index) {
            const auto& item = fci_items_[fci_index];
            ByteWriter<uint16_t>::WriteBigEndian(&buffer[*index + 0], item.first_pid);
            ByteWriter<uint16_t>::WriteBigEndian(&buffer[*index + 2], item.bitmask);
            *index += kFciItemSize;
        }
        assert(*index <= max_size);
    }
    return true;
}

} // namespace rtcp
} // namespace naivert 
