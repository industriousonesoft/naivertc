#ifndef _RTC_RTP_RTCP_FEC_HEADER_WRITER_H_
#define _RTC_RTP_RTCP_FEC_HEADER_WRITER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"

#include <vector>
#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT FecHeaderWriter {
public:
    virtual ~FecHeaderWriter();

    // The maximum number of media packets that can be covered by one FEC packet.
    size_t max_media_packets() const;

    // The maximum number of FEC packets that is supported, per call
    // to ForwardErrorCorrection::EncodeFec().
    size_t max_fec_packets() const;

    // The maximum overhead (in bytes) per packet, due to FEC headers.
    size_t max_packet_overhead() const;

    // Calculates the minimum packet mask size needed (in bytes),
    // given the discrete options of the ULPFEC masks and the bits
    // set in the current packet mask.
    virtual size_t MinPacketMaskSize(const uint8_t* packet_mask, PacketMaskBitIndicator packet_mask_bit_idc) const = 0;

    // The header size (in bytes), given the packet mask size.
    virtual size_t FecHeaderSize(PacketMaskBitIndicator packet_mask_bit_idc) const = 0;

    // Writes FEC header.
    virtual void FinalizeFecHeader(uint16_t seq_num_base,
                                   const uint8_t* packet_mask_data,
                                   PacketMaskBitIndicator packet_mask_bit_idc,
                                   std::shared_ptr<FecPacket> fec_packet,
                                   std::optional<uint32_t> media_ssrc) const = 0;

protected:
    FecHeaderWriter(size_t max_media_packets,
                    size_t max_fec_packets,
                    size_t max_packet_overhead);

    const size_t max_media_packets_;
    const size_t max_fec_packets_;
    const size_t max_packet_overhead_;
};
    
} // namespace naivertc


#endif