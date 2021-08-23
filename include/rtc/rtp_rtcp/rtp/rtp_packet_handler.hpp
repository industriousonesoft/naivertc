#ifndef _RTC_RTP_RTCP_RTP_PACKET_PACER_H_
#define _RTC_RTP_RTCP_RTP_PACKET_PACER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_packet_to_send.hpp"

#include <memory>

namespace naivertc {

class RtpPacketHandler {
public:
    virtual ~RtpPacketHandler() = default;
    
    virtual void EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) = 0;
};

} // namespace naivertc

#endif