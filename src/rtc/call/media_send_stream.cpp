#include "rtc/call/media_send_stream.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator_flex.hpp"
#include "rtc/rtp_rtcp/rtp/fec/fec_generator_ulp.hpp"

#include <plog/Log.h>

namespace naivertc {

MediaSendStream::MediaSendStream(const RtpConfig& rtp_config,
                                         std::shared_ptr<Clock> clock,
                                         std::shared_ptr<Transport> send_transport, 
                                         std::shared_ptr<TaskQueue> task_queue) 
    : rtp_config_(rtp_config),
      clock_(clock),
      task_queue_(task_queue) {
    InitRtpRtcpModules(rtp_config, clock, send_transport, task_queue);
    // TODO: RTT PeriodicUpdate
}
                                         
MediaSendStream::~MediaSendStream() = default;

// Private methods
void MediaSendStream::InitRtpRtcpModules(const RtpConfig& rtp_config,
                                         std::shared_ptr<Clock> clock,
                                         std::shared_ptr<Transport> send_transport,
                                         std::shared_ptr<TaskQueue> task_queue) {
    
    uint32_t local_media_ssrc = rtp_config.local_media_ssrc;
    std::optional<uint32_t> rtx_send_ssrc = rtp_config.rtx_send_ssrc;
    auto fec_generator = MaybeCreateFecGenerator(rtp_config, rtp_config.local_media_ssrc);

    // RtpSender
    RtpSender::Configuration rtp_sender_config;
    rtp_sender_config.audio = media_type() == MediaType::AUDIO;
    rtp_sender_config.extmap_allow_mixed = rtp_config.extmap_allow_mixed;
    rtp_sender_config.local_media_ssrc = local_media_ssrc;
    rtp_sender_config.rtx_send_ssrc = rtx_send_ssrc;
    auto rtp_sender = std::make_shared<RtpSender>(rtp_sender_config, clock, send_transport, fec_generator, task_queue);
    // FIXME: Why do we need to enable NACK here?? What the rtp_config.nack_enabled works for?
    rtp_sender->SetStorePacketsStatus(true, kMinSendSidePacketHistorySize);

    // RtcpSender
    RtcpSender::Configuration rtcp_sender_config;
    rtcp_sender_config.audio = media_type() == MediaType::AUDIO;
    if (rtp_config.rtcp_report_interval_ms) {
        rtcp_sender_config.rtcp_report_interval = TimeDelta::Millis(rtp_config.rtcp_report_interval_ms);
    }
    rtcp_sender_config.local_media_ssrc = local_media_ssrc;
    auto rtcp_sender = std::make_shared<RtcpSender>(rtcp_sender_config, clock, task_queue);
    rtcp_sender->OnNextSendEvaluationTimeScheduled(std::bind(&MediaSendStream::ScheduleRtcpSendEvaluation, this, std::placeholders::_1));

    // RtcpReceiver
    RtcpReceiver::Configuration rtcp_receiver_config;
    rtcp_receiver_config.local_media_ssrc = local_media_ssrc;
    rtcp_receiver_config.rtx_send_ssrc = rtx_send_ssrc;
    rtcp_receiver_config.fec_ssrc = fec_generator->fec_ssrc();
    auto rtcp_receiver = std::make_shared<RtcpReceiver>(rtcp_receiver_config, this, clock, task_queue);

    rtcp_sender_ = std::move(rtcp_sender);
    rtcp_receiver_ = std::move(rtcp_receiver);
    rtp_sender_ = std::move(rtp_sender);
    fec_generator_ = std::move(fec_generator);
}

std::shared_ptr<FecGenerator> MediaSendStream::MaybeCreateFecGenerator(const RtpConfig& rtp_config, uint32_t media_ssrc) {
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
