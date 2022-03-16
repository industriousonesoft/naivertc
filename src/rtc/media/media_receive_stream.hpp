#ifndef _RTC_MEDIA_MEDIA_RECEIVE_STREAM_H_
#define _RTC_MEDIA_MEDIA_RECEIVE_STREAM_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_packet_sink.hpp"

namespace naivertc {

struct RtpParameters;

class MediaReceiveStream : public RtpPacketSink,
                           public RtcpPacketSink {
public:
    virtual ~MediaReceiveStream() = default;
    virtual std::vector<uint32_t> ssrcs() const = 0;
    virtual const RtpParameters* rtp_params() const = 0;
};
    
} // namespace naivertc


#endif