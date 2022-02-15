#ifndef _RTC_TRANSPORT_RTC_MEDIA_TRANSPORT_INTERFACE_H_
#define _RTC_TRANSPORT_RTC_MEDIA_TRANSPORT_INTERFACE_H_

#include "base/defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/base/packet_options.hpp"

namespace naivertc {

// RtcMediaTransport
class RTC_CPP_EXPORT RtcMediaTransport {
public:
    virtual ~RtcMediaTransport() = default;
    virtual int SendRtpPacket(CopyOnWriteBuffer packet, 
                               PacketOptions options, 
                               bool is_rtcp) = 0;    
};

} // namespace naivertc


#endif