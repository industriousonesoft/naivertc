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

    // SSRCs to use for the local media streams.
    std::vector<uint32_t> media_ssrcs;
    // Payload type to use for the local media stream.
    int media_payload_type = -1;

    // If rtx_ssrcs are specified, they should correspond to the media_ssrcs:
    // 1) rtx_ssrcs.size() == 0 || rtx_ssrcs.size() == media_ssrcs.size()
    // 2) if rtx_ssrcs.size() > 0, then rtx_ssrcs[i] should correspond to media_ssrcs[i]
    // SSRCs to use for the RTX streams.
    std::vector<uint32_t> rtx_ssrcs;
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

    bool IsMediaSsrc(uint32_t ssrc) const;
    bool IsRtxSsrc(uint32_t ssrc) const;
    bool IsFlexfecSsrc(uint32_t ssrc) const;
    std::optional<uint32_t> RtxSsrcCorrespondToMediaSsrc(uint32_t media_ssrc) const;
    std::optional<uint32_t> MediaSsrcCorrespondToRtxSsrc(uint32_t media_ssrc) const;
};
    
} // namespace naivertc


#endif