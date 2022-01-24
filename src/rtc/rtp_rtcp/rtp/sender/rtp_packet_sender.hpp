#ifndef _RTC_RTP_RTCP_RTP_PACKET_PACER_H_
#define _RTC_RTP_RTCP_RTP_PACKET_PACER_H_

#include "base/defines.hpp"

namespace naivertc {

class RtpPacketToSend;

class RTC_CPP_EXPORT RtpPacketSender {
public:
    virtual ~RtpPacketSender() = default;
    virtual void EnqueuePackets(std::vector<RtpPacketToSend> packets) = 0;
};
    
} // namespace naivertc


#endif