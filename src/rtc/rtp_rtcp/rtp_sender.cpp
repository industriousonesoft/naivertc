#include "rtc/rtp_rtcp/rtp_sender.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"

#include <plog/Log.h>

namespace naivertc {

RtpSender::RtpSender(const RtpConfiguration& config)
    : rtx_mode_(RtxMode::OFF),
      clock_(config.clock),
      fec_generator_(config.fec_generator),
      packet_history_(std::make_unique<RtpPacketHistory>(config.clock, config.enable_rtx_padding_prioritization)),
      packet_egresser_(std::make_unique<RtpPacketEgresser>(config, packet_history_.get())),
      packet_generator_(std::make_unique<RtpPacketGenerator>(config)),
      packet_sender_(std::make_unique<RtpPacketEgresser::NonPacedPacketSender>(packet_egresser_.get(), packet_generator_.get())) {
    RTC_RUN_ON(&sequence_checker_);
}

RtpSender::~RtpSender() {
    RTC_RUN_ON(&sequence_checker_);
};

size_t RtpSender::max_rtp_packet_size() const {
    RTC_RUN_ON(&sequence_checker_);
    return packet_generator_->max_rtp_packet_size();
}

void RtpSender::set_max_rtp_packet_size(size_t max_size) {
    RTC_RUN_ON(&sequence_checker_);
    packet_generator_->set_max_rtp_packet_size(max_size);
}

RtxMode RtpSender::rtx_mode() const {
    RTC_RUN_ON(&sequence_checker_);
    return rtx_mode_;
}
    
void RtpSender::set_rtx_mode(RtxMode mode) {
    RTC_RUN_ON(&sequence_checker_);
    rtx_mode_ = mode;
}

std::optional<uint32_t> RtpSender::rtx_ssrc() const {
    RTC_RUN_ON(&sequence_checker_);
    return packet_generator_->rtx_ssrc();
}

void RtpSender::SetRtxPayloadType(int payload_type, int associated_payload_type) {
    RTC_RUN_ON(&sequence_checker_);
    packet_generator_->SetRtxPayloadType(payload_type, associated_payload_type);
}

RtpPacketToSend RtpSender::GeneratePacket() const {
    RTC_RUN_ON(&sequence_checker_);
    return packet_generator_->GeneratePacket();
}

bool RtpSender::EnqueuePackets(std::vector<RtpPacketToSend> packets) {
    RTC_RUN_ON(&sequence_checker_);
    int64_t now_ms = clock_->now_ms();
    for (auto& packet : packets) {
        // Assigns sequence number for per packet.
        if (!packet_generator_->AssignSequenceNumber(packet)) {
            PLOG_WARNING << "Failed to assign sequence number for packet with type : " << int(packet.packet_type());
            return false;
        }
        // Sets capture time if necessary.
        if (packet.capture_time_ms() <= 0) {
            packet.set_capture_time_ms(now_ms);
        }
    }
    packet_sender_->EnqueuePackets(std::move(packets));
    return true;
}

bool RtpSender::Register(std::string_view uri, int id) {
    RTC_RUN_ON(&sequence_checker_);
    return packet_generator_->Register(uri, id);
}

bool RtpSender::IsRegistered(RtpExtensionType type) {
    RTC_RUN_ON(&sequence_checker_);
    return packet_generator_->IsRegistered(type);
}

void RtpSender::Deregister(std::string_view uri) {
    RTC_RUN_ON(&sequence_checker_);
    packet_generator_->Deregister(uri);
}

bool RtpSender::AssignSequenceNumber(RtpPacketToSend& packet) {
    RTC_RUN_ON(&sequence_checker_);
    return packet_generator_->AssignSequenceNumber(packet);
}

bool RtpSender::AssignSequenceNumbers(ArrayView<RtpPacketToSend> packets) {
    RTC_RUN_ON(&sequence_checker_);
    for (auto& packet : packets) {
        if (!packet_generator_->AssignSequenceNumber(packet)) {
            return false;
        }
    }
    return true;
}

void RtpSender::SetStorePacketsStatus(const bool enable, const uint16_t number_to_store) {
    RTC_RUN_ON(&sequence_checker_);
    auto storage_mode = enable ? RtpPacketHistory::StorageMode::STORE_AND_CULL
                               : RtpPacketHistory::StorageMode::DISABLE;
    packet_history_->SetStorePacketsStatus(storage_mode, number_to_store);
}

// FEC
bool RtpSender::fec_enabled() const {
    RTC_RUN_ON(&sequence_checker_);
    return fec_generator_ != nullptr;
}

bool RtpSender::red_enabled() const {
    RTC_RUN_ON(&sequence_checker_);
    if (!fec_enabled()) {
        return false;
    }
    return fec_generator_->red_payload_type().has_value();
}

size_t RtpSender::FecPacketOverhead() const {
    RTC_RUN_ON(&sequence_checker_);
    size_t overhead = 0;
    if (!fec_enabled()) {
        return overhead;
    }
    overhead = fec_generator_->MaxPacketOverhead();
    if (fec_generator_->red_payload_type().has_value()) {
        // RED packet overhead 
        overhead += kRedForFecHeaderSize;
        // ULP FEC
        if (fec_generator_->fec_type() == FecGenerator::FecType::ULP_FEC) {
            // For ULPFEC, the overhead is the FEC headers plus RED for FEC header 
            // plus anthing int RTP packet beyond the 12 bytes base header, e.g.:
            // CSRC list, extensions...
            // This reason for the header extensions to be included here is that
            // from an FEC viewpoint, they are part of the payload to be protected.
            // and the base RTP header is already protected by the FEC header.
            overhead += packet_generator_->MaxFecOrPaddingPacketHeaderSize() - kRtpHeaderSize;
        }
    }
    return overhead;
}

// Nack
void RtpSender::OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t rrt_ms) {
    RTC_RUN_ON(&sequence_checker_);
    if (nack_list.empty()) {
        return;
    }
    if (packet_history_->GetStorageMode() == RtpPacketHistory::StorageMode::DISABLE) {
        return;
    }
    // FIXME: Set RTT rrt_ms + 5 ms for keeping more packets in history?
    packet_history_->SetRttMs(5 + rrt_ms);
    for (uint16_t seq_num : nack_list) {
        const int32_t bytes_sent = ResendPacket(seq_num);
        if (bytes_sent < 0) {
            PLOG_WARNING << "Failed resending RTP packet " << seq_num
                         << ", Discard rest of packets.";
            break;
        }
    }
}

// Report blocks
void RtpSender::OnReceivedRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks,
                                              int64_t rtt_ms) {
    RTC_RUN_ON(&sequence_checker_);
    uint32_t media_ssrc = packet_generator_->ssrc();
    std::optional<uint32_t> rtx_ssrc = packet_generator_->rtx_ssrc();

    for (const auto& rb : report_blocks) {
        if (media_ssrc == rb.source_ssrc) {
            // TODO: Received Ack on media ssrc.
        } else if (rtx_ssrc && *rtx_ssrc == rb.source_ssrc) {
            // TODO: Received Ack on rtx ssrc.
        }
    }
}

RtpSendFeedback RtpSender::GetSendFeedback() {
    RTC_RUN_ON(&sequence_checker_);
    RtpSendFeedback send_feedback;
    RtpStreamDataCounters rtp_stats = packet_egresser_->GetRtpStreamDataCounter();
    RtpStreamDataCounters rtx_stats = packet_egresser_->GetRtxStreamDataCounter();
    send_feedback.packets_sent = rtp_stats.transmitted.num_packets + rtx_stats.transmitted.num_packets;
    send_feedback.media_bytes_sent = rtp_stats.transmitted.payload_bytes + rtx_stats.transmitted.payload_bytes;
    send_feedback.send_bitrate = packet_egresser_->GetTotalSendBitrate();
    return send_feedback;
}

// Private methods
int32_t RtpSender::ResendPacket(uint16_t seq_num) {
    // Try to find packet in RTP packet history(Also verify RTT in GetPacketState), 
    // so that we don't retransmit too often.
    std::optional<RtpPacketHistory::PacketState> stored_packet = packet_history_->GetPacketState(seq_num);
    if (!stored_packet.has_value() || stored_packet->pending_transmission) {
        // Packet not found or already queued for retransmission, ignore.
        return 0;
    }

    const int32_t packet_size = static_cast<int32_t>(stored_packet->packet_size);
    const bool rtx_enabled = (rtx_mode_ == RtxMode::RETRANSMITTED);

    auto packet = packet_history_->GetPacketAndMarkAsPending(seq_num, [&](const RtpPacketToSend& stored_packet){
        // TODO: Check if we're overusing retransmission bitrate.
        std::optional<RtpPacketToSend> retransmit_packet;
        // Retransmitted by the RTX stream.
        if (rtx_enabled) {
            retransmit_packet = packet_generator_->BuildRtxPacket(stored_packet);
        }else {
            // Retransmitted by the media stream.
            retransmit_packet = stored_packet;
        }
        if (retransmit_packet) {
            retransmit_packet->set_retransmitted_sequence_number(stored_packet.sequence_number());
        }
        return retransmit_packet;
    });

    if (!packet) {
        return -1;
    }

    packet->set_packet_type((RtpPacketType::RETRANSMISSION));
    // A packet can not be FEC and RTX at the same time.
    packet->set_fec_protection_need(false);
    packet->set_red_protection_need(false);
   
    std::vector<RtpPacketToSend> packets;
    packets.emplace_back(std::move(*packet));
    packet_sender_->EnqueuePackets(std::move(packets));

    return packet_size;
}
    
} // namespace naivertc
