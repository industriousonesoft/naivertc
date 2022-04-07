#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_header_writer_ulp.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

namespace naivertc {
    
UlpFecHeaderWriter::UlpFecHeaderWriter() 
    : FecHeaderWriter(kUlpFecMaxMediaPackets, kMaxFecPackets, kFecLevel0HeaderSize + kFecLevel1HeaderSizeLBitSet) {}

UlpFecHeaderWriter::~UlpFecHeaderWriter() = default;

size_t UlpFecHeaderWriter::MinPacketMaskSize(const uint8_t* packet_mask, size_t packet_mask_size) const {
    return packet_mask_size;
}

size_t UlpFecHeaderWriter::FecHeaderSize(size_t packet_mask_size) const {
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
void UlpFecHeaderWriter::FinalizeFecHeader(uint32_t /* Unused by ULPFEC */,
                                           uint16_t seq_num_base,
                                           const uint8_t* packet_mask_data,
                                           size_t packet_mask_size,
                                           CopyOnWriteBuffer& fec_packet) const {
    // FEC Level 0 header
    uint8_t* data = fec_packet.data();
    // The E bit is the extension flag reserved to indicate any future
    // extension to this specification. It SHALL be set to 0, and SHOULD be
    // ignored by the receiver.
    data[0] &= 0x7F;
    // The L bit indicates whether the long mask is used.  When the L bit is
    // not set, the mask is 16 bits long.  When the L bit is set, the mask
    // is then 48 bits long.
    // Set the L bit
    if (packet_mask_size == kUlpFecPacketMaskSizeLBitSet) {
        data[0] |= 0x40;
    }
    // Clear the L bit
    else {
        data[0] &= 0xBF;
    }

    // Copy length recovery field from temporary location
    memcpy(&data[8], &data[2], 2);
    // Write sequence number base.
    ByteWriter<uint16_t>::WriteBigEndian(&data[2], seq_num_base);

    // FEC level 0 header
    const size_t fec_header_size = FecHeaderSize(packet_mask_size);
    // Set protection length field
    ByteWriter<uint16_t>::WriteBigEndian(&data[kFecLevel0HeaderSize], fec_packet.size() - fec_header_size);
    // Copy the packet mask
    memcpy(&data[12], packet_mask_data, packet_mask_size);
}
    
} // namespace naivertc
