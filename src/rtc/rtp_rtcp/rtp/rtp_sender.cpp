#include "rtc/rtp_rtcp/rtp/rtp_sender.hpp"
#include "common/utils_random.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
    
constexpr size_t kRtpHeaderSize = 12;
constexpr uint16_t kMaxInitRtpSeqNumber = 32767;  // 2^15 -1.

} // namespace 

RtpSender::RtpSender(const RtpRtcpInterface::Configuration& config, 
                     RtpPacketHistory* packet_history, 
                     RtpPacketSender* packet_sender,
                     std::shared_ptr<TaskQueue> task_queue) 
    : task_queue_(task_queue ? task_queue : std::make_shared<TaskQueue>("RtpSender.task.queue")),
    clock_(config.clock),
    ssrc_(config.local_media_ssrc),
    rtx_ssrc_(config.rtx_send_ssrc),
    rtx_mode_(RtxMode::OFF),
    max_packet_size_(kIpPacketSize - 28),
    packet_history_(packet_history),
    paced_sender_(packet_sender),
    sequencer_(config.local_media_ssrc, 
               config.rtx_send_ssrc.value_or(config.local_media_ssrc),
               !config.audio,
               config.clock) {
    UpdateHeaderSizes();
    sequencer_.set_rtx_sequence_num(utils::random::random<uint16_t>(1, kMaxInitRtpSeqNumber));
    sequencer_.set_media_sequence_num(utils::random::random<uint16_t>(1, kMaxInitRtpSeqNumber));
}

RtpSender::~RtpSender() {}

uint32_t RtpSender::ssrc() const {
    return task_queue_->Sync<uint32_t>([this](){
        return this->ssrc_;
    });
}

size_t RtpSender::max_packet_size() const {
    return task_queue_->Sync<size_t>([this](){
        return this->max_packet_size_;
    });
}

void RtpSender::set_max_packet_size(size_t max_size) {
    assert(max_size >= 100);
    assert(max_size <= kIpPacketSize);
    task_queue_->Async([this, max_size](){
        this->max_packet_size_ = max_size;
    });
}

std::optional<uint32_t> RtpSender::rtx_ssrc() const {
    return task_queue_->Sync<std::optional<uint32_t>>([this](){
        return this->rtx_ssrc_;
    });
}

RtxMode RtpSender::rtx_mode() const {
    return task_queue_->Sync<RtxMode>([this](){
        return this->rtx_mode_;
    });
}
    
void RtpSender::set_rtx_mode(RtxMode mode) {
    task_queue_->Async([this, mode](){
        this->rtx_mode_ = mode;
    });
}

void RtpSender::SetRtxPayloadType(int payload_type, int associated_payload_type) {
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

bool RtpSender::SendToNetwork(std::shared_ptr<RtpPacketToSend> packet) {
    return task_queue_->Sync<bool>([this, packet=std::move(packet)](){

        if (!packet->packet_type().has_value()) {
            PLOG_WARNING << "Packet type must be set before sending.";
            return false;
        }

        int64_t now_ms = clock_->TimeInMs();

        if (packet->capture_time_ms() <= 0) {
            packet->set_capture_time_ms(now_ms);
        }

        this->paced_sender_->EnqueuePacket(std::move(packet));

        return true;
    });
}

void RtpSender::EnqueuePackets(std::vector<std::shared_ptr<RtpPacketToSend>> packets) {
    task_queue_->Async([this, packets=std::move(packets)](){
        int64_t now_ms = clock_->TimeInMs();
        for (auto& packet : packets) {
            if (!packet->packet_type().has_value()) {
                PLOG_WARNING << "Packet type must be set before sending.";
                return;
            }
            if (packet->capture_time_ms() <= 0) {
                packet->set_capture_time_ms(now_ms);
            }
        }
        this->paced_sender_->EnqueuePackets(std::move(packets));
    });
}

// Nack
void RtpSender::OnReceivedNack(const std::vector<uint16_t>& nack_list, int64_t avg_rrt) {
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

int32_t RtpSender::ResendPacket(uint16_t packet_id) {
    return task_queue_->Sync<int32_t>([this, packet_id](){
        // Try to find packet in RTP packet history(Also verify RTT in GetPacketState), 
        // so that we don't retransmit too often.
        std::optional<RtpPacketHistory::PacketState> stored_packet = this->packet_history_->GetPacketState(packet_id);
        if (!stored_packet.has_value() || stored_packet->pending_transmission) {
            // Packet not found or already queued for retransmission, ignore.
            return 0;
        }

        const int32_t packet_size = static_cast<int32_t>(stored_packet->packet_size);
        const bool rtx_enabled = (this->rtx_mode_ == RtxMode::RETRANSMITTED);

        auto packet = this->packet_history_->GetPacketAndMarkAsPending(packet_id, [&](const RtpPacketToSend& stored_packet){
            // TODO: Check if we're overusing retransmission bitrate.
            std::shared_ptr<RtpPacketToSend> retransmit_packet;
            // Retransmisson in RTX mode
            if (rtx_enabled) {
                retransmit_packet = BuildRtxPacket(stored_packet);
            }
            // Retransmission in normal mode
            else {
                retransmit_packet = std::make_shared<RtpPacketToSend>(stored_packet);
            }
            if (retransmit_packet) {
                retransmit_packet->set_retransmitted_sequence_number(stored_packet.sequence_number());
            }
            return retransmit_packet;
        });

        if (!packet) {
            return -1;
        }

        packet->set_packet_type((RtpPacketMediaType::RETRANSMISSION));
        // A packet can not be FEC and RTX at the same time.
        packet->set_is_fec_packet(false);
        this->paced_sender_->EnqueuePacket(std::move(packet));

        return packet_size;
    });
}

// Private methods
std::shared_ptr<RtpPacketToSend> RtpSender::BuildRtxPacket(const RtpPacketToSend& packet) {
    if (!rtx_ssrc_.has_value()) {
        PLOG_WARNING << "Failed to build RTX packet withou RTX ssrc.";
        return nullptr;
    }
    auto kv = rtx_payload_type_map_.find(packet.payload_type());
    if (kv == rtx_payload_type_map_.end()) {
        return nullptr;
    }

    auto rtx_packet = std::make_shared<RtpPacketToSend>(max_packet_size_);
    rtx_packet->set_payload_type(kv->second);
    rtx_packet->set_ssrc(rtx_ssrc_.value());

    // Replace sequence number.
    sequencer_.Sequence(*rtx_packet);

    // TODO: Copy header and extensions to RTX packet
    
    return rtx_packet;
}

void RtpSender::UpdateHeaderSizes() {
    const size_t rtp_header_size = kRtpHeaderSize + sizeof(uint32_t) * csrcs_.size();
    // TODO: To update size of RTP header with extensions
}
 
} // namespace naivertc
