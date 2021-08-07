#ifndef _RTC_RTP_PACKETIZER_H_
#define _RTC_RTP_PACKETIZER_H_

#include "base/defines.hpp"
#include "rtc/base/internals.hpp"
#include "rtc/rtp_rtcp/rtp_packetization_config.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketizer {
public:
    static constexpr size_t kDefaultMaximumPayloadSize = kDefaultMtuSize - 12 - 8 - 40; // 1220: SRTP/UDP/IPv6

    struct RTC_CPP_EXPORT PaylaodSizeLimits {
        // Why WebRTC chose RTP max payload size to 1200 bytes?
        // a1: See https://stackoverflow.com/questions/47635545/why-webrtc-chose-rtp-max-packet-size-to-1200-bytes
        // This is an arbitraily selected value to avoid packet fragmentation.
        // There is no any exact science behind this as you can be never sure
        // on the actual limits, however 1200 bytes is a safe value for all kind
        // of networks on the public internet (including somethings like a VPN
        // connection over PPPoE) and for RTP there is no much reason to choose 
        // a bigger value.
        // a2: https://groups.google.com/g/discuss-webrtc/c/gH5ysR3SoZI
        // Anyway, 1200 bytes is 1280 bytes minus the RTP headers minus some bytes 
        // for RTP header extensions minus a few "let's play it safe" bytes. 
        // It'll usually work.
        size_t max_payload_size = 1200;
        ssize_t first_packet_reduction_size = 0;
        ssize_t last_packet_reduction_size = 0;
        ssize_t single_packet_reduction_size = 0;
    };
    
public:
    RtpPacketizer(std::shared_ptr<RtpPacketizationConfig> rtp_config, PaylaodSizeLimits limits);
    virtual ~RtpPacketizer();

    std::shared_ptr<RtpPacketizationConfig> rtp_config() const { return rtp_config_; }

    std::shared_ptr<BinaryBuffer> Packetize(std::shared_ptr<BinaryBuffer> payload, bool marker);

protected:
    static constexpr size_t kRtpHeaderSize = 12;

    std::shared_ptr<RtpPacketizationConfig> rtp_config_;
    PaylaodSizeLimits limits_;
};
    
} // namespace naivertc


#endif 