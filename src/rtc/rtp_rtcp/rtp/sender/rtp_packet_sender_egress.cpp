#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sender_egress.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_header_extensions.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
constexpr uint32_t kTimestampTicksPerMs = 90;
} // namespace

RtpPacketSenderEgress::RtpPacketSenderEgress(const RtpRtcpInterface::Configuration& config, 
                                         std::shared_ptr<RtpPacketSentHistory> packet_history,
                                         std::unique_ptr<FecGenerator> fec_generator,
                                         std::shared_ptr<TaskQueue> task_queue) 
        : clock_(config.clock),
          ssrc_(config.local_media_ssrc),
          rtx_ssrc_(config.rtx_send_ssrc),
          flexfec_ssrc_(std::nullopt),
          packet_history_(packet_history),
          fec_generator_(std::move(fec_generator)),
          task_queue_(task_queue) {
}
 
RtpPacketSenderEgress::~RtpPacketSenderEgress() {

}

 void RtpPacketSenderEgress::SetFecProtectionParameters(const FecProtectionParams& delta_params,
                                                        const FecProtectionParams& key_params) {
    task_queue_->Sync([&](){
        this->pending_fec_params_.emplace(delta_params, key_params);
    });
}

void RtpPacketSenderEgress::SendPacket(std::shared_ptr<RtpPacketToSend> packet) {
    task_queue_->Async([this, packet=std::move(packet)](){
        if (!packet) {
            return;
        }
        if (!HasCorrectSsrc(packet)) {
            return;
        }

        if (packet->packet_type() == RtpPacketType::RETRANSMISSION && !packet->retransmitted_sequence_number().has_value()) {
            PLOG_WARNING << "Retransmission RTP packet can not send without retransmitted sequence number.";
            return;
        }

        // TODO: Update sequence number info map

        if (fec_generator_ && packet->fec_protection_need()) {
            
            std::optional<std::pair<FecProtectionParams, FecProtectionParams>> new_fec_params;
            new_fec_params.swap(pending_fec_params_);
            if (new_fec_params) {
                fec_generator_->SetProtectionParameters(new_fec_params->first /* delta */, new_fec_params->second /* key */);
            }

            // TODO: To generate FEC(ULP or FLEX) packet packetized in RED or sent by new ssrc stream
            if (packet->red_protection_need()) {
                this->fec_generator_->PushMediaPacket(packet);
            }else {
                this->fec_generator_->PushMediaPacket(packet);
            }
            
        }  

        const uint32_t packet_ssrc = packet->ssrc();
        const int64_t now_ms = clock_->TimeInMs();

        // Bug webrtc:7859. While FEC is invoked from rtp_sender_video, and not after
        // the pacer, these modifications of the header below are happening after the
        // FEC protection packets are calculated. This will corrupt recovered packets
        // at the same place. It's not an issue for extensions, which are present in
        // all the packets (their content just may be incorrect on recovered packets).
        // In case of VideoTimingExtension, since it's present not in every packet,
        // data after rtp header may be corrupted if these packets are protected by
        // the FEC.
        int64_t diff_ms = now_ms - packet->capture_time_ms();
        if (packet->HasExtension<rtp::TransmissionTimeOffset>()) {
            packet->SetExtension<rtp::TransmissionTimeOffset>(kTimestampTicksPerMs * diff_ms);
        }

        if (packet->HasExtension<rtp::AbsoluteSendTime>()) {
            packet->SetExtension<rtp::AbsoluteSendTime>(rtp::AbsoluteSendTime::MsTo24Bits(now_ms));
        }

        // TODO: Update VideoTimingExtension?

        const bool is_media = packet->packet_type() == RtpPacketType::AUDIO ||
                            packet->packet_type() == RtpPacketType::VIDEO;
        
        auto transport_seq_num_ext = packet->GetExtension<rtp::TransportSequenceNumber>();
        if (transport_seq_num_ext) {
            SendPacketToNetworkFeedback(transport_seq_num_ext->transport_sequence_number(), packet);
        }

        if (packet->packet_type() != RtpPacketType::PADDING &&
            packet->packet_type() != RtpPacketType::RETRANSMISSION) {
            UpdateDelayStatistics(packet->capture_time_ms(), now_ms, packet_ssrc);
            if (transport_seq_num_ext) {
                OnSendPacket(transport_seq_num_ext->transport_sequence_number(), packet->capture_time_ms(), packet_ssrc);
            }
        }

        const bool send_success = SendPacketToNetwork(packet);

        // Put packet in retransmission history or update pending status even if
        // actual sending fails.
        if (is_media && packet->allow_retransmission()) {
            packet_history_->PutRtpPacket(packet, now_ms);
        } else if (packet->retransmitted_sequence_number()) {
            packet_history_->MarkPacketAsSent(*packet->retransmitted_sequence_number());
        }

        if (send_success) {
            // |media_has_been_sent_| is used by RTPSender to figure out if it can send
            // padding in the absence of transport-cc or abs-send-time.
            // In those cases media must be sent first to set a reference timestamp.
            media_has_been_sent_ = true;

            // TODO(sprang): Add support for FEC protecting all header extensions, add media packet to generator here instead.
            RtpPacketType packet_type = packet->packet_type();
            size_t size = packet->size();
            UpdateRtpStats(now_ms, packet_ssrc, packet_type, size);
        }else {
            // TODO: We should clear the FEC packets if send failed?
        }
    });
}

std::vector<std::shared_ptr<RtpPacketToSend>> RtpPacketSenderEgress::FetchFecPackets() const {
    return task_queue_->Sync<std::vector<std::shared_ptr<RtpPacketToSend>>>([](){
        std::vector<std::shared_ptr<RtpPacketToSend>> packets;
        // TODO: Fetch FEC packets from FEC generator
        return packets;
    });
}

// Private methods
bool RtpPacketSenderEgress::SendPacketToNetwork(std::shared_ptr<RtpPacketToSend> packet) {
    // TODO: Send packet to network by transport
    return false;
}

bool RtpPacketSenderEgress::HasCorrectSsrc(std::shared_ptr<RtpPacketToSend> packet) {
    switch (packet->packet_type())
    {
    case RtpPacketType::AUDIO:
    case RtpPacketType::VIDEO:
        return packet->ssrc() == ssrc_;
    case RtpPacketType::RETRANSMISSION:
    case RtpPacketType::PADDING:
        // Both padding and retransmission must be on either the media
        // or the RTX stream.
        return packet->ssrc() == ssrc_ || packet->ssrc() == rtx_ssrc_;
    case RtpPacketType::FEC:
        // FlexFEC is on separate SSRC, ULPFEC uses media SSRC.
        return packet->ssrc() == ssrc_ || packet->ssrc() == flexfec_ssrc_;;
    }
    return false;
}

void RtpPacketSenderEgress::SendPacketToNetworkFeedback(uint16_t packet_id, std::shared_ptr<RtpPacketToSend> packet) {

}

void RtpPacketSenderEgress::UpdateDelayStatistics(int64_t capture_time_ms, int64_t now_ms, uint32_t ssrc) {

}

void RtpPacketSenderEgress::OnSendPacket(uint16_t packet_id, int64_t capture_time_ms, uint32_t ssrc) {
    if (capture_time_ms <= 0) {
        return;
    }
}

void UpdateRtpStats(int64_t now_ms,
                    uint32_t packet_ssrc,
                    RtpPacketType packet_type,
                    size_t packet_size) {

}
    
} // namespace naivertc
