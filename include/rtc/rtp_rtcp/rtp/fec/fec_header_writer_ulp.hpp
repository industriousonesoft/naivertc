#ifndef _RTC_RTP_RTCP_FEC_HEADER_WRITER_ULP_H_
#define _RTC_RTP_RTCP_FEC_HEADER_WRITER_ULP_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_header_writer.hpp"

namespace naivertc {

class RTC_CPP_EXPORT UlpfecHeaderWriter : public FecHeaderWriter {
public:
    UlpfecHeaderWriter();
    ~UlpfecHeaderWriter() override;

    size_t MinPacketMaskSize(const uint8_t* packet_mask, PacketMaskBitIndicator packet_mask_bit_idc) const override;

    size_t FecHeaderSize(PacketMaskBitIndicator packet_mask_bit_idc) const override;

    void FinalizeFecHeader(uint16_t seq_num_base,
                           const uint8_t* packet_mask_data,
                           PacketMaskBitIndicator packet_mask_bit_idc,
                           std::shared_ptr<FecPacket> fec_packet,
                           std::optional<uint32_t> media_ssrc = std::nullopt /* Unused by ULPFEC */) const override;
};
    
} // namespace naivertc


#endif