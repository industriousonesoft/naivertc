#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_receiver_ulp.hpp"
#include "rtc/base/internals.hpp"

#include <plog/Log.h>

namespace naivertc {

constexpr uint8_t kRedHeaderSize = 1u;

UlpFecReceiver::UlpFecReceiver(uint32_t ssrc, 
                               std::shared_ptr<Clock> clock, 
                               std::weak_ptr<RecoveredPacketReceiver> recovered_packet_receiver) 
    : ssrc_(ssrc),
      clock_(std::move(clock)),
      recovered_packet_receiver_(std::move(recovered_packet_receiver)),
      fec_decoder_(FecDecoder::CreateUlpFecDecoder(ssrc_)) {

    fec_decoder_->OnRecoveredPacket(std::bind(&UlpFecReceiver::OnRecoveredPacket, this, std::placeholders::_1));
}

UlpFecReceiver::~UlpFecReceiver() {
    fec_decoder_->Reset();
}

bool UlpFecReceiver::OnRedPacket(const RtpPacketReceived& rtp_packet, uint8_t ulpfec_payload_type) {
    if (rtp_packet.ssrc() != ssrc_) {
        PLOG_WARNING << "Received RED packet with different SSRC than expected, dropping.";
        return false;
    }
    if (rtp_packet.size() > kIpPacketSize) {
        PLOG_WARNING << "Received RED packet with length exceeds maxmimum typical IP packet size, dropping.";
        return false;
    }

    if (rtp_packet.payload_size() == 0) {
        PLOG_WARNING << "Received a truncated RED packet, dropping.";
        return false;
    }

    // Parse RED header (the first octet of payload data).
    ArrayView<const uint8_t> red_payload = rtp_packet.payload();
    // The highest bit is used as RED end marker, 0 means this is the last 
    // RED header block, 1 means there are more than one blocks. 
    bool is_last_red_block = (red_payload.data()[0] & 0x80) == 0;
    // The payload type of the encapsulated packet by RED.
    uint8_t encapsulated_payload_type = rtp_packet.payload().data()[0] & 0x7f;
    bool is_fec = encapsulated_payload_type == ulpfec_payload_type;
    bool is_recovered = rtp_packet.is_recovered();
    uint32_t ssrc = rtp_packet.ssrc();
    uint16_t seq_num = rtp_packet.sequence_number();

    // Check if there are more than one RED header blocks.
    // We only support one block in a RED packet for FEC.
    if (!is_last_red_block) {
        PLOG_WARNING << "More than one block in received RED packet is not supported.";
        return false;
    }

    ++packet_counter_.num_received_packets;
    packet_counter_.num_received_bytes += rtp_packet.size();
    if (packet_counter_.first_packet_arrival_time_ms == -1) {
        packet_counter_.first_packet_arrival_time_ms = clock_->now_ms();
    }

    CopyOnWriteBuffer encapsulated_packet;
    if (is_fec) {
        ++packet_counter_.num_received_fec_packets;
        // Copy the FEC packet behind the RED header.
        encapsulated_packet.Assign(red_payload.data() + kRedHeaderSize, red_payload.size() - kRedHeaderSize);
    } else {
        // Recover the RED packet to RTP packet.
        encapsulated_packet.EnsureCapacity(rtp_packet.size() - kRedHeaderSize);
        // Copy RTP header
        encapsulated_packet.Assign(rtp_packet.data(), rtp_packet.header_size());

        // Recover payload type field (the lower 7 bits of the second octec in RTP header) 
        // from RED payload type to meida payload type.
        uint8_t& payload_type = encapsulated_packet.data()[1];
        // Reset RED payload type.
        payload_type &= 0x80;
        // Set media payload type.
        payload_type |= encapsulated_payload_type;

        // Copy payload and padding data, after the RED header.
        encapsulated_packet.Append(red_payload.data() + kRedHeaderSize, red_payload.size() - kRedHeaderSize);

        // Send reveived media packet to VCM (Video Coding Module)
        if (auto receiver = recovered_packet_receiver_.lock()) {
            receiver->OnRecoveredPacket(encapsulated_packet);
        }

        // TODO: To zero mutable extensions, but why?
    }

    if (!is_recovered) {
        // Do not pass recovered packets to FEC. Recovered packet might have
        // different set of the RTP header extensions and thus different byte
        // representation than the original packet, That will corrupt
        // FEC calculation.
        fec_decoder_->Decode(ssrc, seq_num, is_fec, std::move(encapsulated_packet));
    }

    return true;
}

void UlpFecReceiver::OnRecoveredPacket(const FecDecoder::RecoveredMediaPacket& recovered_packet) {
    ++packet_counter_.num_recovered_packets;
    if (auto receiver = recovered_packet_receiver_.lock()) {
        receiver->OnRecoveredPacket(recovered_packet.pkt);
    }
}
    
} // namespace naivertc
