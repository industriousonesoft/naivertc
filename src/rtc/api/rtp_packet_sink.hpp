#ifndef _RTC_CALL_RTP_PACKET_SINK_H_
#define _RTC_CALL_RTP_PACKET_SINK_H_

#include "base/defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_received.hpp"

namespace naivertc {

// RtcpPacketSink
class RTC_CPP_EXPORT RtcpPacketSink {
public:
    virtual ~RtcpPacketSink() = default;
    virtual void OnRtcpPacket(CopyOnWriteBuffer in_packet) = 0;
};

// RtpPacketSink
class RTC_CPP_EXPORT RtpPacketSink {
public:
    virtual ~RtpPacketSink() = default;
    virtual void OnRtpPacket(RtpPacketReceived in_packet) = 0;
};

} // namespace naivertc

#endif