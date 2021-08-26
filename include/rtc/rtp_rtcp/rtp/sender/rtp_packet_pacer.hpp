#ifndef _RTC_RTP_RTCP_RTP_PACKET_PACER_H_
#define _RTC_RTP_RTCP_RTP_PACKET_PACER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketPacer {
public:
    virtual ~RtpPacketPacer() = default;
    
    virtual void PacingPackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) = 0;
};
    
} // namespace naivertc


#endif