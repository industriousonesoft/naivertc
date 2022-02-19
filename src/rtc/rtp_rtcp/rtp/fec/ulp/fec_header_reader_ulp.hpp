#ifndef _RTC_RTP_RTCP_RTP_FEC_FEC_HEADER_READER_ULP_H_
#define _RTC_RTP_RTCP_RTP_FEC_FEC_HEADER_READER_ULP_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_header_reader.hpp"

namespace naivertc {

class UlpFecHeaderReader : public FecHeaderReader {
public:
    UlpFecHeaderReader();
    ~UlpFecHeaderReader() override;

    size_t FecHeaderSize(size_t packet_mask_size) const;

    bool ReadFecHeader(FecHeader& fec_header,  CopyOnWriteBuffer&) const override;
    
};

} // namespace naivertc

#endif