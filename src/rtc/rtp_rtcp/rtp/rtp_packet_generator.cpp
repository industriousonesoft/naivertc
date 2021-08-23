#include "rtc/rtp_rtcp/rtp/rtp_packet_generator.hpp"
#include "common/utils_random.hpp"
#include "rtc/base/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
    
constexpr size_t kRtpHeaderSize = 12;
constexpr uint16_t kMaxInitRtpSeqNumber = 32767;  // 2^15 -1.

} // namespace 

RtpPacketGenerator::RtpPacketGenerator(const RtpRtcpInterface::Configuration& config, 
                                       std::shared_ptr<RtpPacketHandler> lower,
                                       std::shared_ptr<RtpPacketHistory> packet_history,
                                       std::shared_ptr<TaskQueue> task_queue) 
    : clock_(config.clock),
    ssrc_(config.local_media_ssrc),
    rtx_ssrc_(config.rtx_send_ssrc),
    rtx_mode_(RtxMode::OFF),
    max_packet_size_(kIpPacketSize - 28),
    lower_(lower),
    packet_history_(packet_history),
    task_queue_(task_queue ? task_queue : std::make_shared<TaskQueue>("RtpPacketGenerator.task.queue")),
    sequencer_(std::make_shared<RtpPacketSequencer>(config.local_media_ssrc, 
               config.rtx_send_ssrc.value_or(config.local_media_ssrc),
               !config.audio,
               config.clock)) {
    UpdateHeaderSizes();
    // Random start, 16bits, can not be 0.
    sequencer_->set_rtx_sequence_num(utils::random::random<uint16_t>(1, kMaxInitRtpSeqNumber));
    sequencer_->set_media_sequence_num(utils::random::random<uint16_t>(1, kMaxInitRtpSeqNumber));
}

RtpPacketGenerator::~RtpPacketGenerator() {}

uint32_t RtpPacketGenerator::ssrc() const {
    return task_queue_->Sync<uint32_t>([this](){
        return this->ssrc_;
    });
}

size_t RtpPacketGenerator::max_packet_size() const {
    return task_queue_->Sync<size_t>([this](){
        return this->max_packet_size_;
    });
}

void RtpPacketGenerator::set_max_packet_size(size_t max_size) {
    assert(max_size >= 100);
    assert(max_size <= kIpPacketSize);
    task_queue_->Async([this, max_size](){
        this->max_packet_size_ = max_size;
    });
}

std::optional<uint32_t> RtpPacketGenerator::rtx_ssrc() const {
    return task_queue_->Sync<std::optional<uint32_t>>([this](){
        return this->rtx_ssrc_;
    });
}

RtxMode RtpPacketGenerator::rtx_mode() const {
    return task_queue_->Sync<RtxMode>([this](){
        return this->rtx_mode_;
    });
}
    
void RtpPacketGenerator::set_rtx_mode(RtxMode mode) {
    task_queue_->Async([this, mode](){
        this->rtx_mode_ = mode;
    });
}

void RtpPacketGenerator::SetRtxPayloadType(int payload_type, int associated_payload_type) {
    task_queue_->Async([this, payload_type, associated_payload_type](){
        // All RTP packet type MUST be less than 127 to distingush with RTCP packet, which MUST be greater than 128.
        // RFC 5761 Multiplexing RTP and RTCP 4. Distinguishable RTP and RTCP Packets
        // https://tools.ietf.org/html/rfc5761#section-4
        assert(payload_type <= 127);
        assert(associated_payload_type <= 127);
        if (payload_type < 0) {
            PLOG_WARNING << "Invalid RTX payload type: " << payload_type;
            return;
        }

        rtx_payload_type_map_[associated_payload_type] = payload_type;
    });
}

void RtpPacketGenerator::EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) {
    task_queue_->Async([this, packets=std::move(packets)](){
        int64_t now_ms = clock_->TimeInMs();
        for (auto& packet : packets) {
            if (packet->capture_time_ms() <= 0) {
                packet->set_capture_time_ms(now_ms);
            }
        }
        if (this->lower_) {
            this->lower_->EnqueuePackets(std::move(packets));
        }
    });
}

// Nack
void RtpPacketGenerator::OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t avg_rrt) {
    task_queue_->Async([this, nack_list=std::move(nack_list), avg_rrt](){
        // FIXME: Why set RTT avg_rrt + 5 ms?
        this->packet_history_->SetRtt(5 + avg_rrt);
        for (uint16_t seq_num : nack_list) {
            const int32_t bytes_sent = ResendPacket(seq_num);
            if (bytes_sent < 0) {
                PLOG_WARNING << "Failed resending RTP packet " << seq_num
                             << ", Discard rest of packets.";
                break;
            }
        }
    });
}

std::shared_ptr<SequenceNumberAssigner> RtpPacketGenerator::sequence_num_assigner() const {
    return task_queue_->Sync<std::shared_ptr<SequenceNumberAssigner>>([this](){
        return this->sequencer_;
    });
}

// Private methods
int32_t RtpPacketGenerator::ResendPacket(uint16_t packet_id) {
    // Try to find packet in RTP packet history(Also verify RTT in GetPacketState), 
    // so that we don't retransmit too often.
    std::optional<RtpPacketHistory::PacketState> stored_packet = this->packet_history_->GetPacketState(packet_id);
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
            retransmit_packet = BuildRtxPacket(stored_packet);
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
    packet->set_is_fec_packet(false);
   
    if (this->lower_) {
        std::vector<std::shared_ptr<RtpPacketToSend>> packets;
        packets.emplace_back(std::move(packet));
        this->lower_->EnqueuePackets(std::move(packets));
    }

    return packet_size;
}

std::shared_ptr<RtpPacketToSend> RtpPacketGenerator::BuildRtxPacket(std::shared_ptr<const RtpPacketToSend> packet) {
    if (!rtx_ssrc_.has_value()) {
        PLOG_WARNING << "Failed to build RTX packet withou RTX ssrc.";
        return nullptr;
    }
    auto kv = rtx_payload_type_map_.find(packet->payload_type());
    if (kv == rtx_payload_type_map_.end()) {
        return nullptr;
    }

    auto rtx_packet = std::make_shared<RtpPacketToSend>(max_packet_size_);
    rtx_packet->set_payload_type(kv->second);
    rtx_packet->set_ssrc(rtx_ssrc_.value());

    // Replace sequence number.
    sequencer_->AssignSequenceNumber(rtx_packet);

    CopyHeaderAndExtensionsToRtxPacket(packet, rtx_packet.get());

    uint8_t* rtx_payload = rtx_packet->AllocatePayload(packet->payload_size() + kRtxHeaderSize);

    // Add original sequence number
    ByteWriter<uint16_t>::WriteBigEndian(rtx_payload, packet->sequence_number());

    // Copy original payload data
    memcpy(rtx_payload + kRtxHeaderSize, packet->payload_data(), packet->payload_size());

    // TODO: To set addtional data if necessary

    // FIXME: Copy capture time so e.g. TransmissionOffset is correctly set. Why?
    rtx_packet->set_capture_time_ms(packet->capture_time_ms());
    
    return rtx_packet;
}

void RtpPacketGenerator::UpdateHeaderSizes() {
    const size_t rtp_header_size = kRtpHeaderSize + sizeof(uint32_t) * csrcs_.size();
    // TODO: To update size of RTP header with extensions
}

void RtpPacketGenerator::CopyHeaderAndExtensionsToRtxPacket(std::shared_ptr<const RtpPacketToSend> packet, RtpPacketToSend* rtx_packet) {
    // Set the relevant fixed fields in the packet headers.
    // The following are not set:
    // - Payload type: it is replaced in RTX packet.
    // - Sequence number: RTX has a seperate sequence numbering.
    // - SSRC: RTX stream has its own SSRC
    rtx_packet->set_marker(packet->marker());
    rtx_packet->set_timestamp(packet->timestamp());

    // Set the variable fields in the packet header
    // - CSRCs: must be set before header extensions
    // - Header extensions: repalce Rid header with RepairedRid header
    const std::vector<uint32_t> csrcs = packet->csrcs();
    rtx_packet->SetCsrcs(csrcs);

    // TODO: Copy header extensions
}
 
} // namespace naivertc
