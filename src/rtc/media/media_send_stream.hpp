#ifndef _RTC_MEDIA_MEDIA_SEND_STREAM_H_
#define _RTC_MEDIA_MEDIA_SEND_STREAM_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_packet_sink.hpp"

namespace naivertc {

class RTC_CPP_EXPORT MediaSendStream : public RtcpPacketSink {
public:
    virtual ~MediaSendStream() = default;
    virtual std::vector<uint32_t> ssrcs() const = 0;    
};
    
} // namespace naivertc


#endif