#ifndef _RTC_CALL_RTP_PACKET_SINK_H_
#define _RTC_CALL_RTP_PACKET_SINK_H_

#include "base/defines.hpp"

namespace naivertc {

class CopyOnWriteBuffer;

class RTC_CPP_EXPORT RtpPacketSink {
    public:
        virtual ~RtpPacketSink() = default;
        virtual void OnRtpPacket(CopyOnWriteBuffer in_packet, bool is_rtcp) = 0;
    };

} // namespace naivertc

#endif