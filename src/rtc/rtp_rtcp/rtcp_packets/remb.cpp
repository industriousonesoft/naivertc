#include "rtc/rtp_rtcp/rtcp_packets/remb.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/common_header.hpp"
#include "rtc/base/byte_io_reader.hpp"
#include "rtc/base/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace rtcp {
constexpr uint32_t kMaxMantissa = 0x3ffff; // 18 bits

// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb, section 2.2).
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P| FMT=15  |   PT=206      |             length            |
//    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  0 |                  SSRC of packet sender                        |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                       Unused = 0                              |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |  Unique identifier 'R' 'E' 'M' 'B'                            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |  Num SSRC     | BR Exp    |  BR Mantissa                      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |   SSRC feedback                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    :  ...   

Remb::Remb() : bitrate_bps_(0) {}

Remb::Remb(const Remb&) = default;

Remb::~Remb() = default;

bool Remb::set_ssrcs(std::vector<uint32_t> ssrcs) {
    if (ssrcs.size() > kMaxNumberOfSsrcs) {
        PLOG_WARNING << "Not enough space for all given SSRCs.";
        return false;
    }
    ssrcs_ = std::move(ssrcs);
    return true;
}

size_t Remb::PacketSize() const {
    return kRtcpCommonHeaderSize + kCommonFeedbackSize + (2 + ssrcs_.size()) * 4;
}

bool Remb::PackInto(uint8_t* buffer,
                    size_t* index,
                    size_t max_size,
                    PacketReadyCallback callback) const {
    while (*index + PacketSize() > max_size) {
        if (!OnBufferFull(buffer, index, callback)) {
            return false;
        }
    }
    if (Psfb::media_ssrc() != 0) {
        PLOG_WARNING << "Media ssrc unused in REMB packet is supposed to be zero.";
        return false;
    }
    size_t index_end = *index + PacketSize();
    // RTCP common header
    RtcpPacket::PackCommonHeader(Psfb::kAfbMessageType, Psfb::kPacketType, PacketSizeWithoutCommonHeader(), buffer, index);
    // Payload-specified common feedback fields
    Psfb::PackCommonFeedback(&buffer[*index]);
    *index += Psfb::kCommonFeedbackSize;

    ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index], kUniqueIdentifier);
    *index += sizeof(uint32_t);

    uint64_t mantissa = bitrate_bps_;
    uint8_t exponent = 0;
    while (mantissa > kMaxMantissa) {
        mantissa >>= 1;
        ++exponent;
    }
    ByteWriter<uint8_t>::WriteBigEndian(&buffer[(*index)++], static_cast<uint8_t>(ssrcs_.size()));
    ByteWriter<uint8_t>::WriteBigEndian(&buffer[(*index)++], (exponent << 2) | mantissa >> 16);
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[*index], mantissa & 0xFFFFu);
    *index += sizeof(uint16_t);

    for (uint32_t ssrc : ssrcs_) {
        ByteWriter<uint32_t>::WriteBigEndian(&buffer[*index], ssrc);
        *index += sizeof(uint32_t);
    }

    assert(index_end == *index && "Unmatched index end.");

    return true;
}

bool Remb::Parse(const CommonHeader& packet) {
    if (packet.type() != Psfb::kPacketType) {
        PLOG_WARNING << "Incoming packet is not a Payload-specified Feedback message.";
        return false;
    }
    if (packet.feedback_message_type() != kAfbMessageType) {
        PLOG_WARNING << "Incoming packet is not a Application layer FB (AFB) message.";
        return false;
    }
    if (packet.payload_size() < kRembBaseSize) {
        PLOG_WARNING << "Payload size " << packet.payload_size()
                     << " is too small for REMB packet.";
        return false;
    }
    const uint8_t* const payload_buffer = packet.payload();
    if (kUniqueIdentifier != ByteReader<uint32_t>::ReadBigEndian(&payload_buffer[8])) {
        PLOG_WARNING << "The unique identifier of REMB packet dose not match.";
        return false;
    }
    uint8_t number_of_ssrcs = payload_buffer[12];
    if (packet.payload_size() != (kCommonFeedbackSize + (2 + number_of_ssrcs) * 4)) {
        PLOG_WARNING << "Payload size " << packet.payload_size()
                     << " does not match " << number_of_ssrcs << " ssrcs.";
        return false;
    }
    // Parse common feedback
    Psfb::ParseCommonFeedback(payload_buffer);

    // BR Exp (6 bits): The exponetial scaling of the mantissa for the 
    // maximun total media bit rate value (ignoring all packet overhead)
    uint8_t br_exponent = payload_buffer[13] >> 2;
    // BR Mantissa (18 bit): The mantissa of the maximum total media bit rate 
    // (ignoring all packet overhead) that the sender of the REMB estimates.
    uint64_t br_mantissa = (static_cast<uint32_t>(payload_buffer[13] & 0x03) << 16) | 
                        ByteReader<uint16_t>::ReadBigEndian(&payload_buffer[14]);
    bitrate_bps_ = (br_mantissa << br_exponent);
    bool shift_overflow = (static_cast<uint64_t>(bitrate_bps_) >> br_exponent) != br_mantissa;
    if (shift_overflow) {
        PLOG_WARNING << "Invalid REMB bitrate value: " << br_mantissa << "*2^" << static_cast<int>(br_exponent);
        return false;
    }

    const uint8_t* next_ssrc = &payload_buffer[16];
    ssrcs_.clear();
    ssrcs_.reserve(number_of_ssrcs);
    for (uint8_t i = 0; i < number_of_ssrcs; i++) {
        ssrcs_.push_back(ByteReader<uint32_t>::ReadBigEndian(next_ssrc));
        next_ssrc += sizeof(uint32_t);
    }

    return true;
}
    
} // namespace rtcp
} // namespace naivertc
