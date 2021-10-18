#include "rtc/rtp_rtcp/rtp/packetizer/rtp_packetizer_h264.hpp"
#include "rtc/media/video/codecs/h264/nalunit.hpp"
#include "rtc/base/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

static constexpr size_t kNaluHeaderSize = 1;
static constexpr size_t kFuAHeaderSize = 2;
static constexpr size_t kLengthFieldSize = 2;

// NAL unit header, RFC 6184, Section 5.3
// +---------------+
// |0|1|2|3|4|5|6|7|
// +-+-+-+-+-+-+-+-+
// |F|NRI|  Type   |
// +---------------+
enum class NaluHeaderBitsMask : uint8_t {
    FORBIDDEN = 0x80,
    NRI = 0x60,
    TYPE = 0x1F
};
using FuAIndicatorBitsMask = NaluHeaderBitsMask;

// NAL unit fragment header, RFC 6184, Section 5.8
// +---------------+
// |0|1|2|3|4|5|6|7|
// +-+-+-+-+-+-+-+-+
// |S|E|R|  Type   |
// +---------------+
enum class FuAHeaderBitsMask : uint8_t {
    START = 0x80,
    END = 0x40,
    RESERVER = 0x20,
    TYPE = 0x1F
};
    
} // namespace

// 参考webrtc: modules/rtp_rtcp/rtp_format_h264.h
RtpH264Packetizer::RtpH264Packetizer() 
        : num_packets_left_(0) {
}

RtpH264Packetizer::~RtpH264Packetizer() {}

size_t RtpH264Packetizer::NumberOfPackets() const {
    return num_packets_left_;
}

bool RtpH264Packetizer::NextPacket(RtpPacketToSend* rtp_packet) {
    if (packet_units_.empty()) {
        return false;
    }
    PacketUnit packet = packet_units_.front();
    if (packet.first_fragment && packet.last_fragment) {
        NextSinglePacket(rtp_packet);
    } else if (packet.aggregated) {
        NextStapAPacket(rtp_packet);
    } else {
        NextFuAPacket(rtp_packet);
    } 
    // Mark the last packet
    rtp_packet->set_marker(packet_units_.empty());
    --num_packets_left_;
    return true;
}

// Private methods
void RtpH264Packetizer::Packetize(ArrayView<const uint8_t> payload, 
                                 const PayloadSizeLimits& limits, 
                                 h264::PacketizationMode packetization_mode) {
    Reset();
    auto nalu_indices = h264::NalUnit::FindNaluIndices(payload.data(), payload.size());
    for (auto& nalu_index : nalu_indices) {
        input_fragments_.push_back(payload.subview(nalu_index.payload_start_offset, nalu_index.payload_size));
    }
    if (!GeneratePackets(limits, packetization_mode)) {
        // If failed to generate all the packets, discard already 
        // generated packts in case the call would ignore return value
        // and still try to call NextPacket
        Reset();
    }
}

bool RtpH264Packetizer::GeneratePackets(const PayloadSizeLimits& limits, h264::PacketizationMode packetization_mode) {
    for (size_t i = 0; i < input_fragments_.size();) {
        if (packetization_mode == h264::PacketizationMode::SINGLE_NAL_UNIT) {
            if (!PacketizeSingleNalu(i, limits)) {
                return false;
            }
            ++i;
        } else if (packetization_mode == h264::PacketizationMode::NON_INTERLEAVED) {
            int fragment_size = input_fragments_[i].size();
            int single_packet_capacity = limits.max_payload_size;
            if (input_fragments_.size() == 1) {
                single_packet_capacity -= limits.single_packet_reduction_size;
            } else if (i == 0) {
                single_packet_capacity -= limits.first_packet_reduction_size;
            } else if (i + 1 == input_fragments_.size()) {
                single_packet_capacity -= limits.last_packet_reduction_size;
            }

            if (fragment_size > single_packet_capacity) {
                if (!PacketizeFuA(i, limits)) {
                    return false;
                }
                ++i;
            } else {
                i = PacketizeStapA(i, limits);
            }
        }
    }
    return true;
}

bool RtpH264Packetizer::PacketizeSingleNalu(size_t fragment_index, const PayloadSizeLimits& limits) {
    size_t payload_size_left = limits.max_payload_size;
    if (input_fragments_.size() == 1) {
        payload_size_left -= limits.single_packet_reduction_size;
    } else if (fragment_index == 0) {
        payload_size_left -= limits.first_packet_reduction_size;
    } else if (fragment_index + 1 == input_fragments_.size()) {
        payload_size_left -= limits.last_packet_reduction_size;
    }

    auto fragment = input_fragments_[fragment_index];
    if (payload_size_left < fragment.size()) {
        PLOG_WARNING << "Failed to fit a fragment to packet in a single NALU"
                     << ", Payload size left: " << payload_size_left
                     << ", fragment size: " << fragment.size()
                     << ", packet capacity: " << limits.max_payload_size;
        return false;
    }
    packet_units_.push(PacketUnit(fragment, true, true, false, fragment[0]));
    ++num_packets_left_;
    return true;
}

bool RtpH264Packetizer::PacketizeFuA(size_t fragment_index, const PayloadSizeLimits& limits) {
    auto fragment = input_fragments_[fragment_index];
    PayloadSizeLimits new_limits = limits;
    // Leave room for the FU-A header
    new_limits.max_payload_size -= kFuAHeaderSize;
    if (input_fragments_.size() != 1) {
        if (fragment_index == input_fragments_.size() - 1) {
            new_limits.single_packet_reduction_size = limits.last_packet_reduction_size;
        } else if (fragment_index == 0) {
            new_limits.single_packet_reduction_size = limits.first_packet_reduction_size;
        } else {
            new_limits.single_packet_reduction_size = 0;
        }
    }
    if (fragment_index != 0) {
        new_limits.first_packet_reduction_size = 0;
    }
    if (fragment_index != input_fragments_.size() - 1) {
        new_limits.last_packet_reduction_size = 0;
    }

    size_t payload_size = fragment.size() - kNaluHeaderSize;
    auto payload_size_list = SplitAboutEqually(payload_size, new_limits);
    if (payload_size_list.empty()) {
        return false;
    }

    size_t offset = kNaluHeaderSize;
    size_t payload_size_left = payload_size;
    for (size_t i = 0; i < payload_size_list.size(); ++i) {
        size_t packet_size = payload_size_list[i];
        packet_units_.push(PacketUnit(fragment.subview(offset, packet_size), i == 0, i == payload_size_list.size() - 1, false, fragment[0]));
        offset += packet_size;
        payload_size_left -= packet_size;
    }
    num_packets_left_ += payload_size_list.size();
    assert(payload_size_left == 0);
    return true;
}

size_t RtpH264Packetizer::PacketizeStapA(size_t fragment_index, const PayloadSizeLimits& limits) {
    size_t payload_size = limits.max_payload_size;
    if (input_fragments_.size() == 1) {
        payload_size -= limits.single_packet_reduction_size;
    } else if (fragment_index == 0) {
        payload_size -= limits.first_packet_reduction_size;
    }
    size_t aggregated_fragments_count = 0;
    size_t fragment_headers_size = 0;

    ArrayView<const uint8_t> fragment = input_fragments_[fragment_index];
    auto payload_size_need = [&] {

        size_t fragment_size = fragment.size() + fragment_headers_size;
        // Single fragment, single packet, payload_size_left already
        // adjusted with limits.single_packet_reduction_size.
        if (input_fragments_.size() == 1) {
            return fragment_size;
        }

        if (fragment_index == input_fragments_.size() - 1) {
            return fragment_size + limits.last_packet_reduction_size;
        }
        return fragment_size;
    };
    
    size_t payload_size_left = payload_size;
    while (payload_size_left >= payload_size_need()) {
        packet_units_.push(PacketUnit(fragment, aggregated_fragments_count == 0, false, true, fragment[0]));
        payload_size_left -= fragment.size();
        payload_size_left -= fragment_headers_size;

        fragment_headers_size = kLengthFieldSize;
        // If we are gonna try to aggregate more fragment into this packet,
        // we need to add the STAP-A NALU header and a length field for the
        // first NALU of this packet
        // 考虑到STAP-A格式下也存在single packet的情况，即某个NALU的大小刚好等于payload_size。
        // 因此此处计算payload_size_need时先不考虑kNaluHeaderSize和kLengthFieldSize，因为
        // 如果是single packet情况是不需要加kNaluHeaderSize和kLengthFieldSize的，所以在计算完
        // 之后在将kNaluHeaderSize和kLengthFieldSize计算payload size的消耗中，用于下一次计算。
        if (aggregated_fragments_count == 0) {
            fragment_headers_size += kNaluHeaderSize + kLengthFieldSize;
        }
        ++aggregated_fragments_count;

        ++fragment_index;
        if (fragment_index == input_fragments_.size()) {
            break;
        }
        fragment = input_fragments_[fragment_index];
    }
    assert(aggregated_fragments_count > 0);
    ++num_packets_left_;
    packet_units_.back().last_fragment = true;
    return fragment_index;
}

// Generate RTP packet payload
void RtpH264Packetizer::NextSinglePacket(RtpPacketToSend* rtp_packet) {
    auto packet = packet_units_.front();
    size_t bytes_to_send = packet.fragment_data.size();
    uint8_t* payload_buffer = rtp_packet->AllocatePayload(bytes_to_send);
    assert(payload_buffer != nullptr);
    memcpy(payload_buffer, packet.fragment_data.data(), bytes_to_send);
    packet_units_.pop();
    input_fragments_.pop_front();
}

// Fragment payload into packets (FU-A)
// e.g.: RTP payload format for FU-A
// 0                   1                   2                   3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// | FU indicator  |   FU header   |                               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
// |                                                               |
// |                         FU payload                            |
// |                                                               |
// |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                               :...OPTIONAL RTP padding        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
void RtpH264Packetizer::NextFuAPacket(RtpPacketToSend* rtp_packet) {
    PacketUnit* packet = &packet_units_.front();
    // NAL unit fragmented over multiple packets (FU-A).
    // We do not send original NALU header, so it will be replaced by the
    // FU indicator header of the first packet.
    uint8_t fu_indicator = (packet->header & 
                            ~uint8_t(FuAIndicatorBitsMask::TYPE) &
                           (uint8_t(FuAIndicatorBitsMask::FORBIDDEN) | 
                            uint8_t(FuAIndicatorBitsMask::NRI))) | 
                            uint8_t(h264::NaluType::FU_A);
    uint8_t fu_header = 0;
    fu_header |= (packet->first_fragment ? uint8_t(FuAHeaderBitsMask::START) : 0);
    fu_header |= (packet->last_fragment ? uint8_t(FuAHeaderBitsMask::END) : 0);
    fu_header |= (packet->header & uint8_t(FuAHeaderBitsMask::TYPE));
    auto fragment = packet->fragment_data;
    uint8_t* payload_buffer = rtp_packet->AllocatePayload(kFuAHeaderSize + fragment.size());
    payload_buffer[0] = fu_indicator;
    payload_buffer[1] = fu_header;
    memcpy(&payload_buffer[kFuAHeaderSize], fragment.data(), fragment.size());
    if (packet->last_fragment) {
        input_fragments_.pop_front();
    }
    packet_units_.pop();
}

// Aggregate fragments into one packet (STAP-A)
// e.g.: An RTP packet including an STAP-A containing two 
// single-time aggregation units
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                          RTP Header                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |STAP-A NAL HDR |         NALU 1 Size           | NALU 1 HDR    |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         NALU 1 Data                           |
// :                                                               :
// +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |               | NALU 2 Size                   | NALU 2 HDR    |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         NALU 2 Data                           |
// :                                                               :
// |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                               :...OPTIONAL RTP padding        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
void RtpH264Packetizer::NextStapAPacket(RtpPacketToSend* rtp_packet) {
    // Reserver maximum available payload, set actual payload size later.
    size_t payload_capacity = rtp_packet->FreeCapacity();
    assert(payload_capacity >= kNaluHeaderSize);
    uint8_t* payload_buffer = rtp_packet->AllocatePayload(payload_capacity);
    assert(payload_buffer != nullptr);
    PacketUnit* packet = &packet_units_.front();
    assert(packet->first_fragment);
    // STAP-A NALU header
    payload_buffer[0] = (packet->header & 
                        ~uint8_t(NaluHeaderBitsMask::TYPE) &
                        (uint8_t(NaluHeaderBitsMask::FORBIDDEN ) | 
                        uint8_t(NaluHeaderBitsMask::NRI))) | 
                        uint8_t(h264::NaluType::STAP_A);
    size_t index = kNaluHeaderSize;
    bool is_last_fragment = packet->last_fragment;
    while (packet->aggregated) {
        auto fragment = packet->fragment_data;
        assert(index + kLengthFieldSize + fragment.size() <= payload_capacity);
        // Add NAL unit length field
        ByteWriter<uint16_t>::WriteBigEndian(&payload_buffer[index], fragment.size());
        index += kLengthFieldSize;
        // Add NAL unit
        memcpy(&payload_buffer[index], fragment.data(), fragment.size());
        index += fragment.size();
        packet_units_.pop();
        input_fragments_.pop_front();
        if (is_last_fragment) {
            break;
        }
        packet = &packet_units_.front();
        is_last_fragment = packet->last_fragment;
    }
    assert(is_last_fragment);
    rtp_packet->SetPayloadSize(index);
}

void RtpH264Packetizer::Reset() {
    while(!packet_units_.empty()) {
        packet_units_.pop();
    }
    input_fragments_.clear();
    num_packets_left_ = 0;
}

} // namespace naivertc
