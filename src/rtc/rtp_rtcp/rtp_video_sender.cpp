#include "rtc/rtp_rtcp/rtp_video_sender.hpp"
#include "rtc/rtp_rtcp/rtp/fec/flex/fec_generator_flex.hpp"
#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_generator_ulp.hpp"

#include <plog/Log.h>

namespace naivertc {

RtpVideoSender::RtpVideoSender(const Configuration& config) 
    : media_payload_type_(config.rtp.media_payload_type) {
    CreateAndInitRtpRtcpModules(config);    
}

RtpVideoSender::~RtpVideoSender() {}

bool RtpVideoSender::OnEncodedFrame(video::EncodedFrame encoded_frame) {
    RTC_RUN_ON(&sequence_checker_);
    // rtp timestamp
    uint32_t rtp_timestamp = rtp_sender_->timestamp_offset() + encoded_frame.timestamp();

    if (!rtcp_responser_->OnReadyToSendRtpFrame(rtp_timestamp, 
                                               encoded_frame.capture_time_ms(),
                                               media_payload_type_,
                                               encoded_frame.frame_type() == video::FrameType::KEY)) {
        return false;
    }

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
    RTC_RUN_ON(&sequence_checker_);
    rtcp_responser_->IncomingRtcpPacket(std::move(in_packet));
}

// Private methods
void RtpVideoSender::CreateAndInitRtpRtcpModules(const Configuration& config) {
    RTC_RUN_ON(&sequence_checker_);
    uint32_t local_media_ssrc = config.rtp.local_media_ssrc;
    std::optional<uint32_t> rtx_send_ssrc = config.rtp.rtx_send_ssrc;
    fec_generator_ = MaybeCreateFecGenerator(config.rtp);

    // RtpSender
    RtpConfiguration rtp_config;
    rtp_config.audio = false;
    rtp_config.extmap_allow_mixed = config.rtp.extmap_allow_mixed;
    rtp_config.local_media_ssrc = local_media_ssrc;
    rtp_config.rtx_send_ssrc = rtx_send_ssrc;
    rtp_config.clock = config.clock;
    rtp_config.send_transport = config.send_transport;
    rtp_config.fec_generator = fec_generator_.get();
    // Observers
    rtp_config.send_delay_observer = config.observers.send_delay_observer;
    rtp_config.send_packet_observer = config.observers.send_packet_observer;
    rtp_config.send_bitrates_observer = config.observers.send_bitrates_observer;
    rtp_config.transport_feedback_observer = config.observers.rtp_transport_feedback_observer;
    rtp_config.stream_data_counters_observer = config.observers.stream_data_counters_observer;
    auto rtp_sender = std::make_unique<RtpSender>(rtp_config);

    // RtcpResponser
    RtcpConfiguration rtcp_config;
    rtcp_config.audio = false;
    rtcp_config.receiver_only = false;
    rtcp_config.rtcp_report_interval_ms = config.rtp.rtcp_report_interval_ms;
    rtcp_config.local_media_ssrc = local_media_ssrc;
    rtcp_config.rtx_send_ssrc = rtx_send_ssrc;
    rtcp_config.fec_ssrc = fec_generator_ ? fec_generator_->fec_ssrc() : std::nullopt;
    rtcp_config.clock = config.clock;
    rtcp_config.send_transport = config.send_transport;
    // Observers
    rtcp_config.packet_type_counter_observer = config.observers.packet_type_counter_observer;
    rtcp_config.intra_frame_observer = config.observers.intra_frame_observer;
    rtcp_config.loss_notification_observer = config.observers.loss_notification_observer;
    rtcp_config.bandwidth_observer = config.observers.bandwidth_observer;
    rtcp_config.cname_observer = config.observers.cname_observer;
    rtcp_config.rtt_observer = config.observers.rtt_observer;
    rtcp_config.transport_feedback_observer = config.observers.rtcp_transport_feedback_observer;
    rtcp_config.nack_list_observer = rtp_sender.get();
    rtcp_config.report_blocks_observer = rtp_sender.get();
    rtcp_config.rtp_send_stats_provider = rtp_sender.get();
    auto rtcp_responser = std::make_unique<RtcpResponser>(rtcp_config);
    
    rtcp_responser_ = std::move(rtcp_responser);
    rtp_sender_ = std::move(rtp_sender);
    sender_video_ = std::make_unique<RtpSenderVideo>(config.clock, rtp_sender_.get());

    // Init
    InitRtpRtcpModules(config.rtp);
}

void RtpVideoSender::InitRtpRtcpModules(const RtpParameters& rtp_params) {
    // RTP sender
    // RTX
    if (rtp_params.media_rtx_payload_type) {
        rtp_sender_->SetRtxPayloadType(*rtp_params.media_rtx_payload_type, rtp_params.media_payload_type);
        rtp_sender_->set_rtx_mode(RtxMode::RETRANSMITTED | RtxMode::REDUNDANT_PAYLOADS);
    }
    // RED + RTX
    if (rtp_params.ulpfec.red_rtx_payload_type) {
        rtp_sender_->SetRtxPayloadType(*rtp_params.ulpfec.red_rtx_payload_type, rtp_params.ulpfec.red_payload_type);
    }
    // Packet History
    rtp_sender_->SetStorePacketsStatus(true, kMinSendSidePacketHistorySize);
    rtp_sender_->set_max_rtp_packet_size(rtp_params.max_packet_size);
    // RTP header extension
    for (const auto& rtp_extension : rtp_params.extensions) {
        rtp_sender_->Register(rtp_extension.uri, rtp_extension.id);
    }

    // RTCP responser
    rtcp_responser_->set_sending(true);
    rtcp_responser_->set_rtcp_mode(RtcpMode::COMPOUND);
    rtcp_responser_->RegisterPayloadFrequency(rtp_params.media_payload_type, kVideoPayloadTypeFrequency);
}

std::unique_ptr<FecGenerator> RtpVideoSender::MaybeCreateFecGenerator(const RtpParameters& rtp_params) {
    RTC_RUN_ON(&sequence_checker_);
    // Flexfec takes priority
    if (rtp_params.flexfec.payload_type >= 0) {
        assert(rtp_params.flexfec.payload_type <= 127);
        if (rtp_params.flexfec.ssrc == 0) {
            PLOG_WARNING << "Disable FlexFEC since no FlexFEC ssrc given.";
            return nullptr;
        }

        if (rtp_params.flexfec.protected_media_ssrc == 0) {
            PLOG_WARNING << "Disable FlexFEC since no protected media ssrc given.";
            return nullptr;
        }

        // TODO: Match flexfec ssrc in suspended ssrcs? but why?

        if (rtp_params.local_media_ssrc != rtp_params.flexfec.protected_media_ssrc) {
            PLOG_WARNING << "Media ssrc not equal to the protected media ssrc.";
            return nullptr;
        }

        return std::make_unique<FlexfecGenerator>(rtp_params.flexfec.payload_type, 
                                                  rtp_params.flexfec.ssrc, 
                                                  rtp_params.flexfec.protected_media_ssrc);

    } else if (rtp_params.ulpfec.red_payload_type >= 0 && 
               rtp_params.ulpfec.ulpfec_payload_type >= 0) {
        // In webrtc: call/rtp_video_sender.cc:105
        // Payload tyeps without picture ID (contained in VP8/VP9, not in H264) cannnot determine
        // that a stream is complete without retransmitting FEC, so using UlpFEC + NACK for H264 
        // is a waste of bandwidth since FEC packtes still have to be transmitted. But that is not
        // the case with FlecFEC.
        // See https://blog.csdn.net/volvet/article/details/53700049
        // FIXME: Is there a way to solve UlpFEC + NACK? ULPFEC sent in a seperated stream, like FlexFEC?
        if (rtp_params.nack_enabled /* TODO: && codec_type == H264*/) {
            PLOG_WARNING << "Transmitting payload type without picture ID using "
                            "NACK+ULPFEC is a waste of bandwidth since ULPFEC packets "
                            "also have to be retransmitted. Disabling ULPFEC.";
            return nullptr;
        }
        return std::make_unique<UlpFecGenerator>(rtp_params.ulpfec.red_payload_type, 
                                                 rtp_params.ulpfec.ulpfec_payload_type);
    }

    return nullptr;
}

} // namespace naivertc
