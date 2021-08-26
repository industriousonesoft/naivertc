#ifndef _RTC_RTP_PACKETIZER_H_
#define _RTC_RTP_PACKETIZER_H_

#include "base/defines.hpp"
#include "rtc/base/internals.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet_to_send.hpp"

#include <vector>

namespace naivertc {

class RTC_CPP_EXPORT RtpPacketizer {
public:
    static constexpr size_t kDefaultMaximumPayloadSize = kDefaultMtuSize - 12 - 8 - 40; // 1220: SRTP/UDP/IPv6
    // Payload size limits
    struct PayloadSizeLimits {
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
    static std::vector<size_t> SplitAboutEqually(size_t payload_size, const PayloadSizeLimits& limits);

public:
    virtual ~RtpPacketizer() = default;

    // Return the number of remaining packets to produce by the packetizer
    virtual size_t NumberOfPackets() const = 0;

    // Return the next packet on success, nilptr otherwise
    virtual bool NextPacket(RtpPacketToSend* rtp_packet) = 0;
};
    
} // namespace naivertc


#endif 