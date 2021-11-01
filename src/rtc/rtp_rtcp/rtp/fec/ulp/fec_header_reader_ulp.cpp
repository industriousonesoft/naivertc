#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_header_reader_ulp.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"
#include "rtc/base/byte_io_reader.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
    
constexpr size_t kPacketMaskOffset = kFecLevel0HeaderSize + 2;
    
} // namespace

UlpFecHeaderReader::UlpFecHeaderReader() 
    : FecHeaderReader(kMaxTrackedMediaPackets, kMaxFecPackets) {}

UlpFecHeaderReader::~UlpFecHeaderReader() {} 

size_t UlpFecHeaderReader::FecHeaderSize(size_t packet_mask_size) const {
    if (packet_mask_size <= kUlpFecPacketMaskSizeLBitClear) {
        return kFecLevel0HeaderSize + kFecLevel1HeaderSizeLBitClear;
    } else {
        return kFecLevel0HeaderSize + kFecLevel1HeaderSizeLBitSet;
    }
}

// https://datatracker.ietf.org/doc/html/rfc5109#section-7.3
// FEC Level 0 Header, 10 octets.
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |E|L|P|X|  CC   |M| PT recovery |            SN base            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          TS recovery                          |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |        length recovery        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// FEC Level 1 Header, 4 octets (L = 0) or 8 octets (L = 1).
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |       Protection Length       |             mask              |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |              mask cont. (present only when L = 1)             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
bool UlpFecHeaderReader::ReadFecHeader(FecHeader& fec_header, CopyOnWriteBuffer& fec_packet) const {
    uint8_t* data = fec_packet.data();
    if (fec_packet.size() < kPacketMaskOffset) {
        PLOG_WARNING << "Truncated FEC packet.";
        return false;
    }
    bool is_l_bit_set = (data[0] & 0x40) != 0u;
    size_t packet_mask_size = is_l_bit_set ? kUlpFecPacketMaskSizeLBitSet : kUlpFecPacketMaskSizeLBitClear;
    fec_header.fec_header_size = FecHeaderSize(packet_mask_size);
    fec_header.seq_num_base = ByteReader<uint16_t>::ReadBigEndian(&data[2]);
    fec_header.packet_mask_offset = kPacketMaskOffset;
    fec_header.packet_mask_size = packet_mask_size;
    fec_header.protection_length = ByteReader<uint16_t>::ReadBigEndian(&data[kFecLevel0HeaderSize /* 10 bytes */]);

    // Store length recovery field in temporary location in header.
    // This makes the header "compatible" with the corresponding
    // FlexFEC location of the length recovery field, thus simplifying
    // the XORing operations.
    memcpy(&data[2], &data[8], 2);

    return true;
}
    
} // namespace naivertc
