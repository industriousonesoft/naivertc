#include "rtc/rtp_rtcp/rtp/fec/fec_decoder.hpp"
#include "rtc/base/numerics/modulo_operator.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
    
constexpr uint16_t kOldSequenceThreshold = 0x3fff;

} // namespace 

FecDecoder::FecDecoder(uint32_t fec_ssrc, 
                       uint32_t protected_media_ssrc, 
                       std::unique_ptr<FecHeaderReader> fec_header_reader) 
    : fec_ssrc_(fec_ssrc),
      protected_media_ssrc_(protected_media_ssrc),
      fec_header_reader_(std::move(fec_header_reader)) {}

FecDecoder::~FecDecoder() {}

void FecDecoder::Decode(uint32_t fec_ssrc, uint16_t seq_num, bool is_fec, CopyOnWriteBuffer received_packet) {
    const size_t max_media_packets = fec_header_reader_->max_media_packets();

    // Reset if there has a big gap in sequence numbers.
    if (recovered_media_packets_.size() >= max_media_packets) {
        const auto& last_recovered_packet = recovered_media_packets_.rbegin();
        // Belongs to the same sequence number space.
        if (fec_ssrc == last_recovered_packet->first) {
            const uint16_t seq_num_diff = MinDiff(seq_num, last_recovered_packet->second.seq_num);
            // A big gap in sequence numbers. The old recovered packets are now
            // useless, so it's safe to do a reset.
            if (seq_num_diff > max_media_packets) {
                PLOG_WARNING << "Big gap in media/UlpFec sequence numbers."
                             << " No need to keep the old packets in the recovered packet buffer,"
                             << " thus resetting them.";
                Reset();
            }
        }
    }

    InsertPacket(fec_ssrc, seq_num, is_fec, std::move(received_packet));
}

// Private methods
void FecDecoder::InsertPacket(uint32_t fec_ssrc, uint16_t seq_num, bool is_fec, CopyOnWriteBuffer received_packet) {
    // Discard old FEC packets such that the sequence numbers in `received_fec_packets_` 
    // span at most 1/2 of the sequence number space.
    // This is important for keeping `received_fec_packets_` sorted, and may also reduce
    // the possiblity of incorrect decoding due to sequence number wrap-around.
    if (!received_fec_packets_.empty() &&
        fec_ssrc == received_fec_packets_.begin()->second.ssrc) {
        // It only makes sense to detect wrap-around when `received_fec_packet` and `front_received_fec_packet`
        // belong to the same sequence number space. i.e., the same SSRC.
        auto it = received_fec_packets_.begin();
        while (it != received_fec_packets_.end()) {
            uint16_t seq_num_diff = MinDiff<uint16_t>(seq_num, it->first);
            if (seq_num_diff > kOldSequenceThreshold) {
                it = received_fec_packets_.erase(it);
            }else {
                // No need to keep iterating, since `received_fec_packets_` is sorted.
                break;
            }
        }
    }

    if (is_fec) {
        InsertFecPacket(fec_ssrc, seq_num, std::move(received_packet));
    } else {
        InsertMediaPacket(fec_ssrc, seq_num, std::move(received_packet));
    }

    DiscardOldRecoveredPackets();
}

void FecDecoder::InsertFecPacket(uint32_t fec_ssrc, uint16_t seq_num, CopyOnWriteBuffer received_packet) {
    // The incoming FEC packet and FEC decoder belong to the same sequence number space.
    assert(fec_ssrc == fec_ssrc_);

    FecPacket fec_packet;
    fec_packet.ssrc = fec_ssrc;
    fec_packet.seq_num = seq_num;
    // TODO: Compatible with FlexFEC.
    // The FEC packet and media packet using the same stream to transport in UlpFEC.
    fec_packet.protected_ssrc = fec_ssrc;
    fec_packet.pkt = std::move(received_packet);

    auto& fec_header = fec_packet.fec_header;

    // Parse ULP/FLX FEC header specific info.
    if (!fec_header_reader_->ReadFecHeader(fec_header, fec_packet.pkt)) {
        return;
    }

    // FIXME: Is this necessary?
    if (fec_packet.protected_ssrc != protected_media_ssrc_) {
        PLOG_WARNING << "Received FEC packet is protecting an unknown media SSRC, dropping.";
        return;
    }

    if (fec_header.packet_mask_offset + fec_header.packet_mask_size > fec_packet.pkt.size()) {
        PLOG_WARNING << "Received a truncated FEC packet, droping.";
        return;
    }

    // Parse packet mask from header and represent as protected packets.
    for (uint16_t byte_idx = 0; byte_idx < fec_header.packet_mask_size; ++byte_idx) {
        uint8_t packet_mask = fec_packet.pkt.data()[fec_header.packet_mask_offset + byte_idx];
        for (uint16_t bit_idx = 0; bit_idx < 8; ++bit_idx) {
            // Mask bit is set.
            if (packet_mask & (1 << (7 - bit_idx))) {
                MediaPacket protected_media_packet;
                protected_media_packet.ssrc = protected_media_ssrc_;
                protected_media_packet.seq_num = static_cast<uint16_t>(fec_header.seq_num_base + (byte_idx << 3 /* byte_idx * 8 (1 byte = 8 bit) */) + bit_idx);
                fec_packet.protected_media_packets[protected_media_packet.seq_num] = std::move(protected_media_packet);
            }
        }
    }

    auto& protected_media_packets = fec_packet.protected_media_packets;
    if (protected_media_packets.empty()) {
        PLOG_WARNING << "Received FEC packet has all-zero packet mask.";
        return;
    }

    // Assign protected packets with recovered packets.
    auto it_p = protected_media_packets.begin();
    auto it_r = recovered_media_packets_.cbegin();
    while (it_p != protected_media_packets.end() && it_r != recovered_media_packets_.end()) {
        // it_r is newer than it_p
        if (wrap_around_utils::AheadOf<uint16_t>(it_r->first, it_p->first)) {
            ++it_p;
        // it_p is newer than it_r
        } else if (wrap_around_utils::AheadOf<uint16_t>(it_p->first, it_r->first)) {
            ++it_r;
        // it_r is equan to it_p
        } else {
            it_p->second.pkt = it_r->second.pkt;
            ++it_p;
            ++it_r;
        }
    }
}

void FecDecoder::InsertMediaPacket(uint32_t media_ssrc, uint16_t seq_num, CopyOnWriteBuffer received_packet) {
    // The incoming media packet belong to the sequence number space protected by decoder.
    assert(media_ssrc == protected_media_ssrc_);

    RecoveredMediaPacket media_packet;
    media_packet.ssrc = media_ssrc;
    media_packet.seq_num = seq_num;
    // This media packet was not recovered by FEC.
    media_packet.was_recovered = false;
    // This media packet has already been passed on.
    media_packet.returned = true;
    media_packet.pkt = std::move(received_packet);

    // Try to recover the protected packet with current media packet.
    UpdateConveringFecPackets(media_packet);

    recovered_media_packets_[seq_num] = std::move(media_packet);
}

void FecDecoder::UpdateConveringFecPackets(const RecoveredMediaPacket& recovered_packet) {
    for (auto& fec_packet : received_fec_packets_) {
        // Find the packet has the same sequence number as the recovered packet.
        auto protected_it = fec_packet.second.protected_media_packets.find(recovered_packet.seq_num);
        // This FEC packet is protecting the recovered media packet.
        if (protected_it != fec_packet.second.protected_media_packets.end()) {
            protected_it->second.pkt = recovered_packet.pkt;
        }
    }
}

void FecDecoder::DiscardOldRecoveredPackets() {
    const size_t max_media_packets = fec_header_reader_->max_media_packets();
    auto it = recovered_media_packets_.begin();
    // NOTE: We can't use `lower_bound` here, since there is not guarantee about
    // all the packet in recovered packets are continuous.
    // Furthermore, the measurement of discarding old packet is based on packet count
    // not sequence number age.
    while (recovered_media_packets_.size() > max_media_packets) {
        it = recovered_media_packets_.erase(it);
    }
}

void FecDecoder::TryToRecover() {
    auto fec_packet_it = received_fec_packets_.begin();
    while (fec_packet_it != received_fec_packets_.end()) {
        int num_packets_to_recover = NumPacketsToRecover(fec_packet_it->second);
        if (num_packets_to_recover == 1) {
            // TODO: Try to recover FEC packet.
        } else if (num_packets_to_recover == 0 || IsOldFecPacket(fec_packet_it->second)) {
            // Either all protected packets arrived or have beed recovered, or the FEC
            // packet is old. We can discard this FEC packet.
            fec_packet_it = received_fec_packets_.erase(fec_packet_it);
        } else {
            ++fec_packet_it;
        }
    }
}

size_t FecDecoder::NumPacketsToRecover(const FecPacket& fec_packet) const {
    size_t packets_to_recover = 0;
    for (const auto& protected_packet : fec_packet.protected_media_packets) {
        // Count the packet we need to recover.
        if (protected_packet.second.pkt.empty()) {
            ++packets_to_recover;
            // We can't recover more than one packet.
            if (packets_to_recover > 1) {
                break;
            }
        }
    }
    return packets_to_recover;
}

bool FecDecoder::IsOldFecPacket(const FecPacket& fec_packet) const {
    if (recovered_media_packets_.empty()) {
        return false;
    }

    const uint16_t last_recovered_seq_num = recovered_media_packets_.rbegin()->first;
    const uint16_t last_protected_seq_num = fec_packet.protected_media_packets.rbegin()->first;

    // `fec_packet` is old if its `last_protected_seq_num` is much older
    // than `last_recovered_seq_num`.
    return MinDiff(last_recovered_seq_num, last_protected_seq_num) > kOldSequenceThreshold;
}

void FecDecoder::Reset() {
    recovered_media_packets_.clear();
    received_fec_packets_.clear();
}
    
} // namespace naivertc