#ifndef _RTC_CALL_RTP_PACKET_SINK_H_
#define _RTC_CALL_RTP_PACKET_SINK_H_

#include "base/defines.hpp"

namespace naivertc {

class CopyOnWriteBuffer;
class RtpPacketReceived;

class RTC_CPP_EXPORT RtpPacketSink {
public:
    virtual ~RtpPacketSink() = default;
    virtual void OnRtcpPacket(CopyOnWriteBuffer in_packet) = 0;
    virtual void OnRtpPacket(RtpPacketReceived in_packet) = 0;
};

} // namespace naivertc

#endif