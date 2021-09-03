#include "rtc/call/video_send_stream.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator_flex.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator_ulp.hpp"

#include <plog/Log.h>

namespace naivertc {

// RtpStream implemention
VideoSendStream::RtpStream::RtpStream(std::unique_ptr<RtpRtcpImpl> rtp_rtcp, 
                                        std::unique_ptr<RtpVideoSender> rtp_video_sender) 
    : rtp_rtcp(std::move(rtp_rtcp)),
      rtp_video_sender(std::move(rtp_video_sender)) {}

VideoSendStream::RtpStream::~RtpStream() = default;

// VideoSendStream implemention
VideoSendStream::VideoSendStream(std::shared_ptr<Clock> clock, 
                                 const RtpConfig& rtp_config,
                                 std::shared_ptr<Transport> send_transport,
                                 std::shared_ptr<TaskQueue> task_queue) 
    : rtp_config_(rtp_config),
      task_queue_(task_queue) {
    InitRtpStreams(clock, rtp_config, send_transport, task_queue);
}

VideoSendStream::~VideoSendStream() {

}

bool VideoSendStream::SendEncodedFrame(std::shared_ptr<VideoEncodedFrame> encoded_frame) {
    
    return true;
}

// Private methods
void VideoSendStream::InitRtpStreams(std::shared_ptr<Clock> clock, 
                                     const RtpConfig& rtp_config,
                                     std::shared_ptr<Transport> send_transport,
                                     std::shared_ptr<TaskQueue> task_queue) {
    RtpRtcpInterface::Configuration configuration;
    configuration.clock = std::move(clock);
    configuration.send_transport = std::move(send_transport);
    configuration.audio = false;
    configuration.rtcp_report_interval_ms = rtp_config.rtcp_report_interval_ms;
    configuration.extmap_allow_mixed = rtp_config.extmap_allow_mixed;

    for (size_t i = 0; i < rtp_config.media_ssrcs.size(); ++i) {
        
        uint32_t local_media_ssrc = rtp_config.media_ssrcs[i];
        auto fec_generator = CreateFecGeneratorIfNecessary(rtp_config, local_media_ssrc);

        // Create rtp_rtcp
        configuration.local_media_ssrc = local_media_ssrc;
        configuration.fec_generator = fec_generator;
        configuration.rtx_send_ssrc = rtp_config.RtxSsrcCorrespondToMediaSsrc(local_media_ssrc);

        auto rtp_rtcp = std::make_unique<RtpRtcpImpl>(configuration, task_queue);
        // FIXME: Why always Enable NACK here?? What the rtp_config.nack_enabled works for?
        rtp_rtcp->SetStorePacketsStatus(true, kMinSendSidePacketHistorySize);

        // TODO: Create RtpVideoSender
        RtpVideoSender::Configuration video_config;
        // video_config.packet_sender = 
        // auto rtp_video_sender = std::make_unique<RtpVideoSender>()

    }
}

std::shared_ptr<FecGenerator> VideoSendStream::CreateFecGeneratorIfNecessary(const RtpConfig& rtp_config, uint32_t media_ssrc) {
    // Flexfec takes priority
    if (rtp_config.flexfec.payload_type >= 0) {
        assert(rtp_config.flexfec.payload_type <= 127);
        if (rtp_config.flexfec.ssrc == 0) {
            PLOG_WARNING << "Disable FlexFEC since no FlexFEC ssrc given.";
            return nullptr;
        }

        if (rtp_config.flexfec.protected_media_ssrc == 0) {
            PLOG_WARNING << "Disable FlexFEC since no protected media ssrc given.";
            return nullptr;
        }

        // TODO: Match flexfec ssrc in suspended ssrcs? but why?

        if (media_ssrc != rtp_config.flexfec.protected_media_ssrc) {
            PLOG_WARNING << "Media ssrc not equal to the protected media ssrc.";
            return nullptr;
        }

        return std::make_unique<FlexfecGenerator>();
    }else if (rtp_config.ulpfec.red_payload_type >= 0 && 
              rtp_config.ulpfec.ulpfec_payload_type >= 0) {
        // Payload tyeps without picture ID (contained in VP8/VP9, not in H264) cannnot determine
        // that a stream is complete without retransmitting FEC, so using UlpFEC + NACK for H264 
        // is a waste of bandwidth since FEC packtes still have to be transmitted. But that is not
        // the case with FlecFEC.
        // See https://blog.csdn.net/volvet/article/details/53700049
        // FIXME: Is there a way to solve UlpFEC + NACK? ULPFEC sent in a seperated stream, like FlexFEC?
        if (rtp_config.nack_enabled) {
            return nullptr;
        }
        return std::make_unique<UlpfecGenerator>(rtp_config.ulpfec.red_payload_type, 
                                                 rtp_config.ulpfec.ulpfec_payload_type);
    }

    return nullptr;
}

} // namespace naivertc
