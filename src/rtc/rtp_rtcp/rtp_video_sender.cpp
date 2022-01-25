#include "rtc/rtp_rtcp/rtp_video_sender.hpp"
#include "rtc/rtp_rtcp/rtp/fec/flex/fec_generator_flex.hpp"
#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_generator_ulp.hpp"

#include <plog/Log.h>

namespace naivertc {

RtpVideoSender::RtpVideoSender(const Configuration& config,
                               Clock* clock,
                               MediaTransport* send_transport) 
    : media_payload_type_(config.media_payload_type),
      clock_(clock) {
    InitRtpRtcpModules(config, clock, send_transport);
}

RtpVideoSender::~RtpVideoSender() {}

bool RtpVideoSender::OnEncodedFrame(video::EncodedFrame encoded_frame) {
    RTC_RUN_ON(&sequence_checker_);
    // rtp timestamp
    uint32_t rtp_timestamp = encoded_frame.timestamp();

    std::optional<int64_t> expected_restransmission_time_ms;
    if (encoded_frame.retransmission_allowed()) {
        expected_restransmission_time_ms = rtcp_responser_->ExpectedRestransmissionTimeMs();
    }

    RtpVideoHeader video_header;
    video_header.frame_type = encoded_frame.frame_type();
    video_header.codec_type = encoded_frame.codec_type();
    video_header.frame_width = encoded_frame.width();
    video_header.frame_height = encoded_frame.height();

    bool bRet = sender_video_->Send(media_payload_type_, 
                                    rtp_timestamp, 
                                    encoded_frame.capture_time_ms(),
                                    video_header,
                                    encoded_frame,
                                    expected_restransmission_time_ms);

    return bRet;
}

void RtpVideoSender::OnRtcpPacket(CopyOnWriteBuffer in_packet) {
    rtcp_responser_->IncomingPacket(std::move(in_packet));
}

// Private methods
void RtpVideoSender::InitRtpRtcpModules(const Configuration& config,
                                        Clock* clock,
                                        MediaTransport* send_transport) {
    RTC_RUN_ON(&sequence_checker_);
    uint32_t local_media_ssrc = config.local_media_ssrc;
    std::optional<uint32_t> rtx_send_ssrc = config.rtx_send_ssrc;
    fec_generator_ = MaybeCreateFecGenerator(config, local_media_ssrc);

    // RtpSender
    RtpConfiguration rtp_config;
    rtp_config.audio = false;
    rtp_config.extmap_allow_mixed = config.extmap_allow_mixed;
    rtp_config.local_media_ssrc = local_media_ssrc;
    rtp_config.rtx_send_ssrc = rtx_send_ssrc;
    rtp_config.clock = clock;
    rtp_config.send_transport = send_transport;
    rtp_config.fec_generator = fec_generator_.get();
    auto rtp_sender = std::make_unique<RtpSender>(rtp_config);
    // FIXME: Why do we need to enable NACK here?? What the rtp_config.nack_enabled works for?
    rtp_sender->SetStorePacketsStatus(true, kMinSendSidePacketHistorySize);

    // RtcpResponser
    RtcpConfiguration rtcp_config;
    rtcp_config.audio = false;
    rtcp_config.rtcp_report_interval_ms = config.rtcp_report_interval_ms;
    rtcp_config.local_media_ssrc = local_media_ssrc;
    rtcp_config.rtx_send_ssrc = rtx_send_ssrc;
    rtcp_config.fec_ssrc = fec_generator_ ? fec_generator_->fec_ssrc() : std::nullopt;
    rtcp_config.clock = clock;
    rtcp_config.rtp_send_feedback_provider = rtp_sender.get();
    auto rtcp_responser = std::make_unique<RtcpResponser>(rtcp_config);
   
    rtcp_responser_ = std::move(rtcp_responser);
    rtp_sender_ = std::move(rtp_sender);
    sender_video_ = std::make_unique<RtpSenderVideo>(clock, rtp_sender_.get());
}

std::unique_ptr<FecGenerator> RtpVideoSender::MaybeCreateFecGenerator(const Configuration& config, uint32_t media_ssrc) {
    RTC_RUN_ON(&sequence_checker_);
    // Flexfec takes priority
    if (config.flexfec.payload_type >= 0) {
        assert(config.flexfec.payload_type <= 127);
        if (config.flexfec.ssrc == 0) {
            PLOG_WARNING << "Disable FlexFEC since no FlexFEC ssrc given.";
            return nullptr;
        }

        if (config.flexfec.protected_media_ssrc == 0) {
            PLOG_WARNING << "Disable FlexFEC since no protected media ssrc given.";
            return nullptr;
        }

        // TODO: Match flexfec ssrc in suspended ssrcs? but why?

        if (media_ssrc != config.flexfec.protected_media_ssrc) {
            PLOG_WARNING << "Media ssrc not equal to the protected media ssrc.";
            return nullptr;
        }

        return std::make_unique<FlexfecGenerator>(config.flexfec.payload_type, config.flexfec.ssrc, media_ssrc);

    } else if (config.ulpfec.red_payload_type >= 0 && 
               config.ulpfec.ulpfec_payload_type >= 0) {
        // Payload tyeps without picture ID (contained in VP8/VP9, not in H264) cannnot determine
        // that a stream is complete without retransmitting FEC, so using UlpFEC + NACK for H264 
        // is a waste of bandwidth since FEC packtes still have to be transmitted. But that is not
        // the case with FlecFEC.
        // See https://blog.csdn.net/volvet/article/details/53700049
        // FIXME: Is there a way to solve UlpFEC + NACK? ULPFEC sent in a seperated stream, like FlexFEC?
        if (config.nack_enabled) {
            return nullptr;
        }
        return std::make_unique<UlpFecGenerator>(config.ulpfec.red_payload_type, 
                                                 config.ulpfec.ulpfec_payload_type);
    }

    return nullptr;
}

} // namespace naivertc
