#ifndef _RTC_RTP_RTCP_FEC_HEADER_WRITER_ULP_H_
#define _RTC_RTP_RTCP_FEC_HEADER_WRITER_ULP_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_header_writer.hpp"

namespace naivertc {

class UlpFecHeaderWriter : public FecHeaderWriter {
public:
    UlpFecHeaderWriter();
    ~UlpFecHeaderWriter() override;

    size_t MinPacketMaskSize(const uint8_t* packet_mask, size_t packet_mask_size) const override;

    size_t FecHeaderSize(size_t packet_mask_size) const override;

    void FinalizeFecHeader(uint32_t media_ssrc,
                           uint16_t seq_num_base,
                           const uint8_t* packet_mask_data,
                           size_t packet_mask_size,
                           CopyOnWriteBuffer& fec_packet) const override;
};
    
} // namespace naivertc


#endif