#ifndef _RTC_API_MEDIA_TRANSPORT_INTERFACE_H_
#define _RTC_API_MEDIA_TRANSPORT_INTERFACE_H_

#include "base/defines.hpp"
#include "rtc/base/copy_on_write_buffer.hpp"
#include "rtc/base/packet_options.hpp"

namespace naivertc {

// MediaTransport
class RTC_CPP_EXPORT MediaTransport {
public:
    virtual int SendRtpPacket(CopyOnWriteBuffer packet, PacketOptions options) = 0;
    virtual int SendRtcpPacket(CopyOnWriteBuffer packet, PacketOptions options) = 0;
protected:
    virtual ~MediaTransport() = default;
};

    
} // namespace naivertc


#endif