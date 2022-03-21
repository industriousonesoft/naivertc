#include "rtc/rtp_rtcp/rtp_sender.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_egresser.hpp"
#include "common/utils_random.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr uint16_t kMaxInitRtpSeqNumber = 32767;  // 2^15 - 1
    
} // namespace

// RtpSenderContext
RtpSender::RtpSenderContext::RtpSenderContext(const RtpConfiguration& config) 
    : packet_sequencer(config),
      packet_history(config.clock, config.enable_rtx_padding_prioritization),
      packet_generator(config, &packet_history),
      packet_egresser(config, &packet_history),
      non_paced_sender(&packet_egresser, &packet_sequencer) {}

// RtpSender
RtpSender::RtpSender(const RtpConfiguration& config)
    : clock_(config.clock),
      ctx_(std::make_unique<RtpSenderContext>(config)),
      fec_generator_(config.fec_generator),
      paced_sender_(config.paced_sender ? config.paced_sender : &ctx_->non_paced_sender) {
    RTC_RUN_ON(&sequence_checker_);

    timestamp_offset_ = utils::random::generate_random<uint32_t>();

    // Random start, 16bits, can not be 0.
    ctx_->packet_sequencer.set_rtx_seq_num(utils::random::random<uint16_t>(1, kMaxInitRtpSeqNumber));
    ctx_->packet_sequencer.set_media_seq_num(utils::random::random<uint16_t>(1, kMaxInitRtpSeqNumber));
}

RtpSender::~RtpSender() {
    RTC_RUN_ON(&sequence_checker_);
};

uint32_t RtpSender::timestamp_offset() const {
    RTC_RUN_ON(&sequence_checker_);
    return timestamp_offset_;
}

size_t RtpSender::max_rtp_packet_size() const {
    RTC_RUN_ON(&sequence_checker_);
    return ctx_->packet_generator.max_rtp_packet_size();
}

void RtpSender::set_max_rtp_packet_size(size_t max_size) {
    RTC_RUN_ON(&sequence_checker_);
    ctx_->packet_generator.set_max_rtp_packet_size(max_size);
}

int RtpSender::rtx_mode() const {
    RTC_RUN_ON(&sequence_checker_);
    return ctx_->packet_generator.rtx_mode();
}
    
void RtpSender::set_rtx_mode(int mode) {
    RTC_RUN_ON(&sequence_checker_);
    ctx_->packet_generator.set_rtx_mode(mode);
}

std::optional<uint32_t> RtpSender::rtx_ssrc() const {
    RTC_RUN_ON(&sequence_checker_);
    return ctx_->packet_generator.rtx_ssrc();
}

void RtpSender::SetRtxPayloadType(int payload_type, int associated_payload_type) {
    RTC_RUN_ON(&sequence_checker_);
    ctx_->packet_generator.SetRtxPayloadType(payload_type, associated_payload_type);
}

RtpPacketToSend RtpSender::GeneratePacket() const {
    RTC_RUN_ON(&sequence_checker_);
    return ctx_->packet_generator.GeneratePacket();
}

bool RtpSender::EnqueuePacket(RtpPacketToSend packet) {
    RTC_RUN_ON(&sequence_checker_);

    // Assigns sequence number for per packet.
    if (!ctx_->packet_sequencer.AssignSequenceNumber(packet)) {
        PLOG_WARNING << "Failed to assign sequence number for packet with type : " << int(packet.packet_type());
        return false;
    }

    // Sets capture time if necessary.
    if (packet.capture_time_ms() <= 0) {
        packet.set_capture_time_ms(clock_->now_ms());
    }

    std::vector<RtpPacketToSend> packets;
    packets.push_back(std::move(packet));
    paced_sender_->EnqueuePackets(std::move(packets));
    return true;
}

bool RtpSender::EnqueuePackets(std::vector<RtpPacketToSend> packets) {
    RTC_RUN_ON(&sequence_checker_);
    int64_t now_ms = clock_->now_ms();
    // TODO: Optimization: move the oprations below to downstream?
    for (auto& packet : packets) {
        // Assigns sequence number for per packet.
        if (!ctx_->packet_sequencer.AssignSequenceNumber(packet)) {
            PLOG_WARNING << "Failed to assign sequence number for packet with type : " << int(packet.packet_type());
            return false;
        }
        // Sets capture time if necessary.
        if (packet.capture_time_ms() <= 0) {
            packet.set_capture_time_ms(now_ms);
        }
    }
    paced_sender_->EnqueuePackets(std::move(packets));
    return true;
}

bool RtpSender::Register(std::string_view uri, int id) {
    RTC_RUN_ON(&sequence_checker_);
    return ctx_->packet_generator.Register(uri, id);;
}

bool RtpSender::IsRegistered(RtpExtensionType type) {
    RTC_RUN_ON(&sequence_checker_);
    return ctx_->packet_generator.IsRegistered(type);
}

void RtpSender::Deregister(std::string_view uri) {
    RTC_RUN_ON(&sequence_checker_);
    ctx_->packet_generator.Deregister(uri);
}

void RtpSender::SetStorePacketsStatus(const bool enable, const uint16_t number_to_store) {
    RTC_RUN_ON(&sequence_checker_);
    auto storage_mode = enable ? RtpPacketHistory::StorageMode::STORE_AND_CULL
                               : RtpPacketHistory::StorageMode::DISABLE;
    ctx_->packet_history.SetStorePacketsStatus(storage_mode, number_to_store);
}

void RtpSender::SetSequenceNumberOffset(uint16_t seq_num) {
    RTC_RUN_ON(&sequence_checker_);
    if (ctx_->packet_sequencer.media_seq_num() != seq_num) {
        ctx_->packet_sequencer.set_media_seq_num(seq_num);
        // Clear the packet history since any packet there may conflict with
        // new one.
        ctx_->packet_history.Clear();
    }
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
            overhead += ctx_->packet_generator.MaxFecOrPaddingPacketHeaderSize() - kRtpHeaderSize;
        }
    }
    return overhead;
}

std::vector<RtpPacketToSend> RtpSender::FetchFecPackets() const {
    RTC_RUN_ON(&sequence_checker_);
    return ctx_->packet_egresser.FetchFecPackets();
}

// Padding
std::vector<RtpPacketToSend> RtpSender::GeneratePadding(size_t target_packet_size, 
                                                        bool media_has_been_sent,
                                                        bool can_send_padding_on_media_ssrc) {
    RTC_RUN_ON(&sequence_checker_);
    return ctx_->packet_generator.GeneratePadding(target_packet_size,
                                                  media_has_been_sent,
                                                  can_send_padding_on_media_ssrc);
}

// Nack
void RtpSender::OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t rrt_ms) {
    RTC_RUN_ON(&sequence_checker_);
    if (nack_list.empty()) {
        return;
    }
    if (ctx_->packet_history.GetStorageMode() == RtpPacketHistory::StorageMode::DISABLE) {
        return;
    }
    // FIXME: Set RTT rrt_ms + 5 ms for keeping more packets in history?
    ctx_->packet_history.SetRttMs(5 + rrt_ms);
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
void RtpSender::OnReceivedRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks) {
    RTC_RUN_ON(&sequence_checker_);
    uint32_t media_ssrc = ctx_->packet_generator.media_ssrc();
    std::optional<uint32_t> rtx_ssrc = ctx_->packet_generator.rtx_ssrc();

    for (const auto& rb : report_blocks) {
        if (media_ssrc == rb.source_ssrc) {
            ctx_->packet_generator.OnReceivedAckOnMediaSsrc();
        } else if (rtx_ssrc && *rtx_ssrc == rb.source_ssrc) {
            ctx_->packet_generator.OnReceivedAckOnRtxSsrc();
        }
    }
}

RtpSendStats RtpSender::GetSendStats() {
    RTC_RUN_ON(&sequence_checker_);
    RtpSendStats send_feedback;
    RtpStreamDataCounters rtp_stats = ctx_->packet_egresser.GetRtpStreamDataCounter();
    RtpStreamDataCounters rtx_stats = ctx_->packet_egresser.GetRtxStreamDataCounter();
    send_feedback.packets_sent = rtp_stats.transmitted.num_packets + rtx_stats.transmitted.num_packets;
    send_feedback.media_bytes_sent = rtp_stats.transmitted.payload_bytes + rtx_stats.transmitted.payload_bytes;
    send_feedback.send_bitrate = ctx_->packet_egresser.GetTotalSendBitrate();
    return send_feedback;
}

// Private methods
int32_t RtpSender::ResendPacket(uint16_t seq_num) {
    // Try to find packet in RTP packet history(Also verify RTT in GetPacketState), 
    // so that we don't retransmit too often.
    std::optional<RtpPacketHistory::PacketState> stored_packet = ctx_->packet_history.GetPacketState(seq_num);
    if (!stored_packet.has_value() || stored_packet->pending_transmission) {
        // Packet not found or already queued for retransmission, ignore.
        return 0;
    }

    const int32_t packet_size = static_cast<int32_t>(stored_packet->packet_size);
    const bool rtx_enabled = (rtx_mode() & kRtxRetransmitted);

    auto packet = ctx_->packet_history.GetPacketAndMarkAsPending(seq_num, [&](const RtpPacketToSend& stored_packet){
        // TODO: Check if we're overusing retransmission bitrate.
        std::optional<RtpPacketToSend> retransmit_packet;
        // Retransmitted by the RTX stream.
        if (rtx_enabled) {
            retransmit_packet = ctx_->packet_generator.BuildRtxPacket(stored_packet);
            if (retransmit_packet) {
                // Replace with RTX sequence number.
                ctx_->packet_sequencer.AssignSequenceNumber(*retransmit_packet);
            }
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
    paced_sender_->EnqueuePackets(std::move(packets));

    return packet_size;
}
    
} // namespace naivertc
