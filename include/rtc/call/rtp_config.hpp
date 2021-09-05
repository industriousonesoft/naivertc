#ifndef _RTC_CALL_RTP_CONFIG_H_
#define _RTC_CALL_RTP_CONFIG_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"

#include <vector>
#include <optional>

namespace naivertc {

constexpr size_t kDefaultMaxPacketSize = kIpPacketSize - kTransportOverhead;

struct RTC_CPP_EXPORT RtpConfig {
    RtpConfig();
    RtpConfig(const RtpConfig&);
    ~RtpConfig();

    // SSRC to use for the local media stream.
    uint32_t local_media_ssrc = 0;
    // Payload type to use for the local media stream.
    int media_payload_type = -1;

    std::optional<uint32_t> rtx_send_ssrc = std::nullopt;
    // Payload type to use for the RTX stream.
    int rtx_payload_type = -1;

    // Corresponds to the SDP attribute extmap-allow-mixed
    bool extmap_allow_mixed = false;

    std::vector<rtp::HeaderExtension> extensions;

    // Time interval between RTCP report for video: 1000 ms
    // Time interval between RTCP report for audio: 5000 ms
    size_t rtcp_report_interval_ms = 0;

    size_t max_packet_size = kDefaultMaxPacketSize;

    // NACK
    bool nack_enabled = false;

    // TODO: Ulpfec and flexfex support both of two ways to send: 1) packetized in RED, 2) by a separate stream
    // Ulpfec: RED
    struct Ulpfec {
        Ulpfec();
        Ulpfec(const Ulpfec&);
        ~Ulpfec();

        // Payload type used for ULPFEC packets.
        int ulpfec_payload_type = -1;

        // Payload type used for RED packets.
        int red_payload_type = -1;

        // RTX payload type for RED payload.
        int red_rtx_payload_type = -1;
    } ulpfec;

    // Flexfec: Separate stream
    struct Flexfec {
        Flexfec();
        Flexfec(const Flexfec&);
        ~Flexfec();
        // Payload type of FlexFEC. Set to -1 to disable sending FlexFEC.
        int payload_type = -1;

        // SSRC of FlexFEC stream.
        uint32_t ssrc = 0;

        // The media stream being protected by this FlexFEC stream.
        uint32_t protected_media_ssrc = 0;
    } flexfec;
};
    
} // namespace naivertc


#endif