#ifndef _RTC_RTP_PACKETIZER_H_
#define _RTC_RTP_PACKETIZER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_packetization_config.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketizer {
public:
    RtpPacketizer(std::shared_ptr<RtpPacketizationConfig> rtp_config);
    virtual ~RtpPacketizer();

    std::shared_ptr<RtpPacketizationConfig> rtp_config() const { return rtp_config_; }

    virtual std::shared_ptr<BinaryBuffer> Build(bool marker, std::shared_ptr<BinaryBuffer> payload);

protected:
    static constexpr size_t kRtpHeaderSize = 12;

    std::shared_ptr<RtpPacketizationConfig> rtp_config_;
};
    
} // namespace naivertc


#endif 