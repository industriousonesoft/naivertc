#ifndef _RTC_CALL_VIDEO_SENDER_H_
#define _RTC_CALL_VIDEO_SENDER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtcp_module.hpp"
#include "rtc/rtp_rtcp/rtp_sender.hpp"
#include "rtc/rtp_rtcp/rtp_sender_video.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator.hpp"
#include "rtc/media/video/encoded_frame.hpp"
#include "rtc/media/video/common.hpp"
#include "rtc/api/media_transport.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

#include <vector>

namespace naivertc {

constexpr size_t kDefaultMaxPacketSize = kIpPacketSize - kTransportOverhead;

// RtpVideoSender
class RTC_CPP_EXPORT RtpVideoSender {
public:
    struct Configuration {
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

        // TODO: UlpFec and flexfex support both of two ways to send: 1) packetized in RED, 2) by a separate stream
        // UlpFec: RED
        struct UlpFec {
            // Payload type used for ULPFEC packets.
            int ulpfec_payload_type = -1;

            // Payload type used for RED packets.
            int red_payload_type = -1;

            // RTX payload type for RED payload.
            int red_rtx_payload_type = -1;
        } ulpfec;

        // Flexfec: Separate stream
        struct Flexfec {
            // Payload type of FlexFEC. Set to -1 to disable sending FlexFEC.
            int payload_type = -1;

            // SSRC of FlexFEC stream.
            uint32_t ssrc = 0;

            // The media stream being protected by this FlexFEC stream.
            uint32_t protected_media_ssrc = 0;
        } flexfec;
    };
public:
    RtpVideoSender(const Configuration& config,
                   Clock* clock,
                   MediaTransport* send_transport);
    ~RtpVideoSender();

    bool OnEncodedFrame(video::EncodedFrame encoded_frame);

private:
    void InitRtpRtcpModules(const Configuration& config,
                            Clock* clock,
                            MediaTransport* send_transport);

    std::unique_ptr<FecGenerator> MaybeCreateFecGenerator(const Configuration& config, uint32_t media_ssrc);

private:
    SequenceChecker sequence_checker_;
    const int media_payload_type_;
    Clock* const clock_;
    std::unique_ptr<RtcpModule> rtcp_module_ = nullptr;
    std::unique_ptr<RtpSender> rtp_sender_ = nullptr;
    std::unique_ptr<RtpSenderVideo> sender_video_ = nullptr;
};

} // namespace naivertc


#endif