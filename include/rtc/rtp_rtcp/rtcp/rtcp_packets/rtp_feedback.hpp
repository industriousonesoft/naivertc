#ifndef _RTC_RTCP_RTP_FEEDBACK_H_
#define _RTC_RTCP_RTP_FEEDBACK_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packet.hpp"

namespace naivertc {
namespace rtcp {

class RTC_CPP_EXPORT RtpFeedback : public RtcpPacket {
public:
    static constexpr uint8_t kPacketType = 205;

    RtpFeedback() = default;
    ~RtpFeedback() = default;

    uint32_t media_ssrc() const { return media_ssrc_; }
    void set_media_ssrc(uint32_t ssrc) { media_ssrc_ = ssrc; }

protected:
    static constexpr size_t kCommonFeedbackSize = 8;
    bool ParseCommonFeedback(const uint8_t* buffer, size_t size);
    bool PackCommonFeedbackInto(uint8_t* buffer, size_t size) const;

private:
    uint32_t media_ssrc_ = 0;
};
    
} // namespace rtcp
} // namespace naivertc


#endif