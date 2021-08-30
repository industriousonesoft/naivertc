#ifndef _RTC_RTP_RTCP_FEC_CODEC_H_
#define _RTC_RTP_RTCP_FEC_CODEC_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet.hpp"

namespace naivertc {

class RTC_CPP_EXPORT FecCodec {
public:
    virtual ~FecCodec() = default;
public:
    static void XorHeader(size_t src_protected_size, 
                          const RtpPacket* const src,
                          FecPacket* dst);
    static void XorPayload(size_t src_payload_offset, 
                           size_t dst_payload_offset, 
                           size_t payload_size, 
                           const RtpPacket* const src, 
                           FecPacket* dst);
};
    
} // namespace naivertc


#endif