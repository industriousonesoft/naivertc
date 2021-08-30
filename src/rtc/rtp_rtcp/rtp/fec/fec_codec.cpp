#include "rtc/rtp_rtcp/rtp/fec/fec_codec.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"

namespace naivertc {

void FecCodec::XorHeader(size_t src_protected_size, const RtpPacket* const src, FecPacket* dst) {
    uint8_t* dst_data = dst->data();
    const uint8_t* src_data = src->data();

    // XOR the first 2 bytes of the header: V, P, X, CC, M, PT fields.
    dst_data[0] ^= src_data[0];
    dst_data[1] ^= src_data[1];

    // XOR the length recovery field.
    uint16_t payload_size = static_cast<uint16_t>(src_protected_size);
    dst_data[2] ^= (payload_size >> 8);
    dst_data[3] ^= (payload_size & 0x0F);

    // XOR the 5th to 8th bytes of the header: the timestamp field.
    dst_data[4] ^= src_data[4];
    dst_data[5] ^= src_data[5];
    dst_data[6] ^= src_data[6];
    dst_data[7] ^= src_data[7];

    // Skip the 9th to 12th bytes of the header
}   
    
void FecCodec::XorPayload(size_t src_payload_offset,
                          size_t dst_payload_offset,
                          size_t payload_size, 
                          const RtpPacket* const src,
                          FecPacket* dst) {
    assert(src_payload_offset + payload_size <= src->size());
    assert(dst_payload_offset + payload_size <= dst->capacity());

    if (dst_payload_offset + payload_size > dst->size()) {
        dst->resize(dst_payload_offset + payload_size);
    }

    uint8_t* dst_data = dst->data();
    const uint8_t* src_data = src->data();
    for (size_t i = 0; i < payload_size; ++i) {
        dst_data[dst_payload_offset + i] ^= src_data[src_payload_offset + i];
    }
}
    
} // namespace naivertc
