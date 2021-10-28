#ifndef _RTC_RTP_RTCP_FEC_HEADER_WRITER_H_
#define _RTC_RTP_RTCP_FEC_HEADER_WRITER_H_

#include "base/defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"

#include <vector>
#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT FecHeaderWriter {
public:
    virtual ~FecHeaderWriter();

    // The maximum number of media packets that can be covered by one FEC packet.
    size_t max_media_packets() const { return max_media_packets_; };

    // The maximum number of FEC packets that is supported, per call
    // to ForwardErrorCorrection::EncodeFec().
    size_t max_fec_packets() const { return max_fec_packets_; };

    // The maximum overhead (in bytes) per packet, due to FEC headers.
    size_t max_packet_overhead() const { return max_packet_overhead_; };

    // Calculates the minimum packet mask size needed (in bytes),
    // given the discrete options of the ULPFEC masks and the bits
    // set in the current packet mask.
    virtual size_t MinPacketMaskSize(const uint8_t* packet_mask, size_t packet_mask_size) const = 0;

    // The header size (in bytes), given the packet mask size.
    virtual size_t FecHeaderSize(size_t packet_mask_size) const = 0;

    // Writes FEC header.
    virtual void FinalizeFecHeader(uint32_t media_ssrc,
                                   uint16_t seq_num_base,
                                   const uint8_t* packet_mask_data,
                                   size_t packet_mask_size,
                                   CopyOnWriteBuffer& fec_packet) const = 0;

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