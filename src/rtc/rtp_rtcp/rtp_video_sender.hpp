#ifndef _RTC_CALL_VIDEO_SENDER_H_
#define _RTC_CALL_VIDEO_SENDER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/base/rtp_rtcp_defines.hpp"
#include "rtc/rtp_rtcp/rtcp_responser.hpp"
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
        // SSRC used for the local media stream.
        uint32_t local_media_ssrc = 0;
        // Payload type used for media payload on the media stream.
        int media_payload_type = -1;
        // RTX payload type used for media payload on the RTX stream.
        std::optional<int> media_rtx_payload_type = -1;

        std::optional<uint32_t> rtx_send_ssrc = std::nullopt;

        // Corresponds to the SDP attribute extmap-allow-mixed
        bool extmap_allow_mixed = false;
        
        // The default time interval between RTCP report for video: 1000 ms
        // The default time interval between RTCP report for audio: 5000 ms
        size_t rtcp_report_interval_ms = 1000;

        size_t max_packet_size = kDefaultMaxPacketSize;

        // NACK
        bool nack_enabled = false;

        // ULP_FEC + RED
        struct UlpFec {
            // Payload type used for ULP_FEC packets.
            int ulpfec_payload_type = -1;
            // Payload type used for RED packets.
            int red_payload_type = -1;
            // RTX payload type used for RED payload.
            std::optional<int> red_rtx_payload_type = -1;
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

    void OnRtcpPacket(CopyOnWriteBuffer in_packet);

private:
    void CreateAndInitRtpRtcpModules(const Configuration& config,
                                     Clock* clock,
                                     MediaTransport* send_transport);

    void InitRtpRtcpModules(const Configuration& config);

    std::unique_ptr<FecGenerator> MaybeCreateFecGenerator(const Configuration& config, uint32_t media_ssrc);

private:
    SequenceChecker sequence_checker_;
    Clock* const clock_;
    const int media_payload_type_;

    std::unique_ptr<RtcpResponser> rtcp_responser_ = nullptr;
    std::unique_ptr<RtpSender> rtp_sender_ = nullptr;
    std::unique_ptr<RtpSenderVideo> sender_video_ = nullptr;
    std::unique_ptr<FecGenerator> fec_generator_ = nullptr;
};

} // namespace naivertc


#endif