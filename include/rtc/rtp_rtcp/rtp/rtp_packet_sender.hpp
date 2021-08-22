#ifndef _RTC_RTP_RTCP_RTP_PACKET_SENDER_H_
#define _RTC_RTP_RTCP_RTP_PACKET_SENDER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/rtp_packet_to_send.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketSender {
public:
    ~RtpPacketSender() = default;
    
    virtual void SendPacket(std::shared_ptr<RtpPacketToSend> packet) = 0;
    virtual std::vector<std::shared_ptr<RtpPacketToSend>> FetchFecPackets() const;
};
    
} // namespace naivertc


#endif