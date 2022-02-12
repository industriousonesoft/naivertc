#ifndef _RTC_MEDIA_MEDIA_RECEIVE_STREAM_H_
#define _RTC_MEDIA_MEDIA_RECEIVE_STREAM_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_packet_sink.hpp"

namespace naivertc {

class RTC_CPP_EXPORT MediaReceiveStream : public RtpPacketSink,
                                          public RtcpPacketSink {
public:
    virtual ~MediaReceiveStream() = default;
    virtual std::vector<uint32_t> ssrcs() const = 0;
};
    
} // namespace naivertc


#endif