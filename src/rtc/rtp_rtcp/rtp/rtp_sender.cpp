#include "rtc/rtp_rtcp/rtp/rtp_sender.hpp"
#include "common/utils_random.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sent_history.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_sender.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_generator.hpp"
#include "rtc/rtp_rtcp/rtp/sender/rtp_packet_pacer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
    constexpr uint16_t kMaxInitRtpSeqNumber = 32767;  // 2^15 -1.
} // namespace

RtpSender::RtpSender(const Configuration& config, 
                     std::shared_ptr<Clock> clock, 
                     std::shared_ptr<Transport> send_transport, 
                     std::shared_ptr<FecGenerator> fec_generator, 
                     std::shared_ptr<TaskQueue> task_queue)
    : rtx_mode_(RtxMode::OFF),
      clock_(clock),
      fec_generator_(fec_generator),
      task_queue_(task_queue),
      packet_sequencer_(std::make_unique<RtpPacketSequencer>(config, clock)),
      packet_history_(std::make_unique<RtpPacketSentHistory>(config, clock, task_queue)),
      packet_sender_(std::make_unique<RtpPacketSender>(config, packet_history_.get(), clock, fec_generator, task_queue)), 
      packet_generator_(std::make_unique<RtpPacketGenerator>(config, task_queue)),
      non_paced_sender_(std::make_unique<NonPacedPacketSender>(this)) {
      
    // Random start, 16bits, can not be 0.
    packet_sequencer_->set_rtx_sequence_num(utils::random::random<uint16_t>(1, kMaxInitRtpSeqNumber));
    packet_sequencer_->set_media_sequence_num(utils::random::random<uint16_t>(1, kMaxInitRtpSeqNumber));
}

RtpSender::~RtpSender() = default;

size_t RtpSender::max_rtp_packet_size() const {
    return task_queue_->Sync<size_t>([&](){
        return this->packet_generator_->max_rtp_packet_size();
    });
}

void RtpSender::set_max_rtp_packet_size(size_t max_size) {
    task_queue_->Async([&, max_size](){
        this->packet_generator_->set_max_rtp_packet_size(max_size);
    });
}

RtxMode RtpSender::rtx_mode() const {
    return task_queue_->Sync<RtxMode>([&](){
        return this->rtx_mode_;
    });
}
    
void RtpSender::set_rtx_mode(RtxMode mode) {
    task_queue_->Async([&, mode](){
        this->rtx_mode_ = mode;
    });
}

std::optional<uint32_t> RtpSender::rtx_ssrc() const {
    return task_queue_->Sync<std::optional<uint32_t>>([&](){
        return this->packet_generator_->rtx_ssrc();
    });
}

void RtpSender::SetRtxPayloadType(int payload_type, int associated_payload_type) {
    task_queue_->Async([&, payload_type, associated_payload_type](){
        this->packet_generator_->SetRtxPayloadType(payload_type, associated_payload_type);
    });
}

std::shared_ptr<RtpPacketToSend> RtpSender::AllocatePacket() const {
    return task_queue_->Sync<std::shared_ptr<RtpPacketToSend>>([&](){
        return this->packet_generator_->AllocatePacket();
    });
    
}

bool RtpSender::EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) {
    return task_queue_->Sync<bool>([this, packets=std::move(packets)](){
        int64_t now_ms = this->clock_->TimeInMs();
        for (auto& packet : packets) {
            // Assign sequence for per packet
            if (!this->packet_sequencer_->AssignSequenceNumber(packet)) {
                PLOG_WARNING << "Failed to assign sequence number for packet with type : " << int(packet->packet_type());
                return false;
            }
            if (packet->capture_time_ms() <= 0) {
                packet->set_capture_time_ms(now_ms);
            }
        }
        this->non_paced_sender_->EnqueuePackets(std::move(packets));
        return true;
    });
}

// Nack
void RtpSender::OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t avg_rrt) {
    task_queue_->Async([this, nack_list=std::move(nack_list), avg_rrt](){
        // FIXME: Why set RTT avg_rrt + 5 ms?
        this->packet_history_->SetRtt(5 + avg_rrt);
        for (uint16_t seq_num : nack_list) {
            const int32_t bytes_sent = this->ResendPacket(seq_num);
            if (bytes_sent < 0) {
                PLOG_WARNING << "Failed resending RTP packet " << seq_num
                             << ", Discard rest of packets.";
                break;
            }
        }
    });
}

void RtpSender::SetStorePacketsStatus(const bool enable, const uint16_t number_to_store) {
    task_queue_->Async([this, enable, number_to_store](){
        auto storage_mode = enable ? RtpPacketSentHistory::StorageMode::STORE_AND_CULL
                                   : RtpPacketSentHistory::StorageMode::DISABLE;
        this->packet_history_->SetStorePacketsStatus(storage_mode, number_to_store);
    });
}

// FEC
bool RtpSender::fec_enabled() const {
    return task_queue_->Sync<bool>([&](){
        return fec_generator_ != nullptr;
    });
}

bool RtpSender::red_enabled() const {
    return task_queue_->Sync<bool>([&](){
        if (!fec_enabled()) {
            return false;
        }
        return fec_generator_->red_payload_type().has_value();
    });
}

size_t RtpSender::FecPacketOverhead() const {
    return task_queue_->Sync<size_t>([&](){
        size_t overhead = 0;
        if (!this->fec_enabled()) {
            return overhead;
        }
        overhead = this->fec_generator_->MaxPacketOverhead();
        if (this->fec_generator_->red_payload_type().has_value()) {
            // RED packet overhead 
            overhead += kRedForFecHeaderSize;
            // ULP FEC
            if (this->fec_generator_->fec_type() == FecGenerator::FecType::ULP_FEC) {
                // For ULPFEC, the overhead is the FEC headers plus RED for FEC header 
                // plus anthing int RTP packet beyond the 12 bytes base header, e.g.:
                // CSRC list, extensions...
                // This reason for the header extensions to be included here is that
                // from an FEC viewpoint, they are part of the payload to be protected.
                // and the base RTP header is already protected by the FEC header.
                overhead += this->packet_generator_->FecOrPaddingPacketMaxRtpHeaderSize() - kRtpHeaderSize;
            }
        }
        return overhead;
    });
}

// Private methods
int32_t RtpSender::ResendPacket(uint16_t packet_id) {
    // Try to find packet in RTP packet history(Also verify RTT in GetPacketState), 
    // so that we don't retransmit too often.
    std::optional<RtpPacketSentHistory::PacketState> stored_packet = this->packet_history_->GetPacketState(packet_id);
    if (!stored_packet.has_value() || stored_packet->pending_transmission) {
        // Packet not found or already queued for retransmission, ignore.
        return 0;
    }

    const int32_t packet_size = static_cast<int32_t>(stored_packet->packet_size);
    const bool rtx_enabled = (this->rtx_mode_ == RtxMode::RETRANSMITTED);

    auto packet = this->packet_history_->GetPacketAndMarkAsPending(packet_id, [&](std::shared_ptr<RtpPacketToSend> stored_packet){
        // TODO: Check if we're overusing retransmission bitrate.
        std::shared_ptr<RtpPacketToSend> retransmit_packet;
        // Retransmisson in RTX mode
        if (rtx_enabled) {
            retransmit_packet = this->packet_generator_->BuildRtxPacket(stored_packet);
            // Replace sequence number.
            packet_sequencer_->AssignSequenceNumber(retransmit_packet);
        }
        // Retransmission in normal mode
        else {
            retransmit_packet = stored_packet;
        }
        if (retransmit_packet) {
            retransmit_packet->set_retransmitted_sequence_number(stored_packet->sequence_number());
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
   
    std::vector<std::shared_ptr<RtpPacketToSend>> packets;
    packets.emplace_back(std::move(packet));
    this->non_paced_sender_->EnqueuePackets(std::move(packets));

    return packet_size;
}
    
} // namespace naivertc
