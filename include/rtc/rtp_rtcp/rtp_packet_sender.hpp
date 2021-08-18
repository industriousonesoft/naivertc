#ifndef _RTC_RTP_RTCP_RTP_PACKET_SENDER_H_
#define _RTC_RTP_RTCP_RTP_PACKET_SENDER_H_

#include "base/defines.hpp"

#include <memory>

namespace naivertc {

class RtpPacketSender {
public:
    virtual ~RtpPacketSender() = default;

    // Insert a set of packets into queue, for eventual transmission. Based on the
    // type of packets, they will be prioritized and scheduled relative to other
    // packets and the current target send rate.
    virtual void EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) = 0;

    virtual void EnqueuePacket(std::shared_ptr<RtpPacketToSend> packet) = 0;
};

} // namespace naivertc

#endif