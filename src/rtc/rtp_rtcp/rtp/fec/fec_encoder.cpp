#include "rtc/rtp_rtcp/rtp/fec/fec_encoder.hpp"
#include "rtc/rtp_rtcp/rtp/fec/ulp/fec_header_writer_ulp.hpp"
#include "rtc/rtp_rtcp/rtp_rtcp_defines.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace internal {

// CopyColumn
void CopyColumn(size_t num_fec_packets,
                size_t old_mask_size, 
                size_t new_mask_size, 
                size_t old_bit_index,
                size_t new_bit_index, 
                uint8_t* old_masks, 
                uint8_t* new_masks) {
    assert(new_bit_index < new_mask_size * 8);

    for (size_t row = 0; row < num_fec_packets; ++row) {
        size_t new_col_index = row * new_mask_size + new_bit_index / 8;
        size_t old_col_index = row * old_mask_size + old_bit_index / 8;
        // FIXME: How to understand the algorithm?
        new_masks[new_col_index] |= ((old_masks[old_col_index] & 0x80) >> 7);
     
        if (new_bit_index % 8 != 7) {
            new_masks[new_col_index] <<= 1;
        }
        old_masks[old_col_index] <<= 1;
    }
}

void InsertZeroColumns(size_t num_zeros,
                       size_t num_fec_packets, 
                       size_t new_mask_bytes, 
                       size_t new_bit_index, 
                       uint8_t* new_masks) {
    for (size_t row = 0; row < num_fec_packets; ++row) {
        const size_t new_byte_index = row * new_mask_bytes + new_bit_index / 8;
        const size_t max_shifts = (7 - (new_bit_index % 8));
        new_masks[new_byte_index] <<= std::min(num_zeros, max_shifts);
    }
}
    
} // namespace internal

std::unique_ptr<FecEncoder> FecEncoder::CreateUlpFecEncoder() {
    auto fec_header_writer = std::make_unique<UlpFecHeaderWriter>();
    return std::unique_ptr<FecEncoder>(new FecEncoder(std::move(fec_header_writer)));
}

FecEncoder::FecEncoder(std::unique_ptr<FecHeaderWriter> fec_header_writer) 
    : fec_header_writer_(std::move(fec_header_writer)),
      packet_mask_generator_(std::make_unique<FecPacketMaskGenerator>()),
      packet_mask_size_(0) {}

FecEncoder::~FecEncoder() = default;

bool FecEncoder::Encode(const PacketList& media_packets, 
                        uint8_t protection_factor, 
                        size_t num_important_packets, 
                        bool use_unequal_protection, 
                        FecMaskType fec_mask_type,
                        FecPacketList& generated_fec_packets) {
    const size_t num_media_packets = media_packets.size();
    if (num_media_packets == 0) {
        return false;
    }
    if (num_important_packets > num_media_packets) {
        return false;
    }

    const size_t max_media_packets = fec_header_writer_->max_media_packets();
    if (num_media_packets > max_media_packets) {
        PLOG_WARNING << "Can not protect " << num_media_packets
                     << " media packets per frame greater than "
                     << max_media_packets << ".";
        return false;
    }
    
    // Sanity check for media packets
    for (const auto& media_packet : media_packets) {
        if (media_packet.size() < kRtpHeaderSize) {
            PLOG_WARNING << "Media packet size " << media_packet.size()
                         << " is smaller than RTP fixed header size.";
            return false;
        }

        // Ensure the FEC packets will fit in a typical MTU
        if (media_packet.size() + fec_header_writer_->max_packet_overhead() + kTransportOverhead > kIpPacketSize) {
            PLOG_WARNING << "Media packet size " << media_packet.size()
                         << " bytes with overhead is larger than "
                         << kIpPacketSize << " bytes.";
        }
    }

    // Prepare generated FEC packets
    size_t num_fec_packets = CalcNumFecPackets(num_media_packets, protection_factor);
    if (num_fec_packets == 0) {
        return true;
    }
    // Resize
    generated_fec_packets.resize(num_fec_packets);
    packet_mask_size_ = FecPacketMaskGenerator::PacketMaskSize(num_fec_packets);
    memset(packet_masks_, 0, num_fec_packets * packet_mask_size_);
    packet_mask_generator_->GeneratePacketMasks(fec_mask_type, 
                                                num_media_packets, 
                                                num_fec_packets, 
                                                num_important_packets, 
                                                use_unequal_protection, 
                                                packet_masks_);
    size_t num_mask_bits = InsertZeroInPacketMasks(media_packets, num_fec_packets);
    if (num_mask_bits < 0) {
        PLOG_INFO << "Due to sequence number gap, cannot protect media packets with a single block of FEC packets";
        return false;
    }
    // One mask bit to a media packet
    packet_mask_size_ = FecPacketMaskGenerator::PacketMaskSize(num_mask_bits);

    GenerateFecPayload(media_packets, num_fec_packets, generated_fec_packets);

    const auto& first_madia_packet = media_packets.front();
    const uint32_t media_ssrc = first_madia_packet.ssrc();
    const uint16_t seq_num_base = first_madia_packet.sequence_number();
    FinalizeFecHeaders(packet_mask_size_, media_ssrc, seq_num_base, num_fec_packets, generated_fec_packets);
    
    return true;
}

size_t FecEncoder::MaxFecPackets() const {
    return fec_header_writer_->max_fec_packets();
}

size_t FecEncoder::MaxPacketOverhead() const {
    return fec_header_writer_->max_packet_overhead();
}

size_t FecEncoder::CalcNumFecPackets(size_t num_media_packets, uint8_t protection_factor) {
    // Result in Q0 with an unsigned round. (四舍五入)
    size_t num_fec_packets = (num_media_packets * protection_factor + (1 << 7)) >> 8;
    // Generate at least one FEC packet if we need protection.
    if (protection_factor > 0 && num_fec_packets == 0) {
        num_fec_packets = 1;
    }
    return num_fec_packets;
}

// Private methods

// https://datatracker.ietf.org/doc/html/rfc5109#section-7.3
// FEC Level 0 Header, 10 bytes.
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |E|L|P|X|  CC   |M| PT recovery |            SN base            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          TS recovery                          |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |        length recovery        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// FEC Level 1 Header, 4 bytes (L = 0) or 8 bytes (L = 1).
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |       Protection Length       |             mask              |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |              mask cont. (present only when L = 1)             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
void FecEncoder::GenerateFecPayload(const PacketList& media_packets, 
                                    size_t num_fec_packets, 
                                    std::vector<CopyOnWriteBuffer>& generated_fec_packets) {
    for (size_t row = 0; row < num_fec_packets; ++row) {
        CopyOnWriteBuffer& fec_packet = generated_fec_packets[row];
        size_t pkt_mask_idx = row * packet_mask_size_;
        const size_t min_packet_mask_size = fec_header_writer_->MinPacketMaskSize(&packet_masks_[pkt_mask_idx], packet_mask_size_);
        const size_t fec_header_size = fec_header_writer_->FecHeaderSize(min_packet_mask_size);
        const size_t media_header_size = kRtpHeaderSize;

        size_t media_pkt_idx = 0;
        size_t media_packet_payload_size = 0;
        auto media_packets_it = media_packets.cbegin();
        uint16_t prev_seq_num = media_packets_it->sequence_number();
        while (media_packets_it != media_packets.end()) {
            auto& media_packet = *media_packets_it;
            // Rtp header extensons + csrcs + payload data
            media_packet_payload_size = media_packet.size() - media_header_size;

            // The current media packet is protected by FEC packets
            if (packet_masks_[pkt_mask_idx] & (1 << (7 - media_pkt_idx))) {
                bool is_first_protected_packet = (fec_packet.size() == 0);
                size_t fec_packet_size = fec_header_size + media_packet_payload_size;
                if (fec_packet_size > fec_packet.size()) {
                    // The prior XORs are still correct after we expand the packet size.
                    fec_packet.Resize(fec_packet_size);
                }
                
                // Initialized the fec packet for the current row
                // with the first protected media packet.
                if (is_first_protected_packet) {
                    const uint8_t* media_packet_data = media_packet.data();
                    uint8_t* fec_packet_data = fec_packet.data();
                    // Write fec header
                    // Write P, X, CC, M, and PT recovery fields.
                    // Note that bits 0, 1, and 16 are overwritten in FinalizeFecHeaders.
                    memcpy(&fec_packet_data[0], &media_packet_data[0], 0x02);
                    // Write length recovery field. (This is a temporary location for ULPFEC.)
                    ByteWriter<uint16_t>::WriteBigEndian(&fec_packet_data[2], media_packet_payload_size);
                    // Write timestamp recovery field
                    memcpy(&fec_packet_data[4], &media_packet_data[4], 0x04);

                    // Write payload
                    if (media_packet_payload_size > 0) {
                        memcpy(&fec_packet_data[fec_header_size], &media_packet_data[media_header_size], media_packet_payload_size);
                    }
                } else {
                    XorHeader(media_packet, media_packet_payload_size, fec_packet);
                    XorPayload(media_header_size, 
                               media_packet_payload_size, 
                               media_packet, 
                               fec_header_size,
                               fec_packet);
                }
            }
            media_packets_it++;
            if (media_packets_it != media_packets.end()) {
                uint16_t curr_seq_num = media_packets_it->sequence_number();
                // Skip the missing media packets
                media_pkt_idx += curr_seq_num - prev_seq_num;
                prev_seq_num = curr_seq_num;
            }
            // Mask byte offset
            pkt_mask_idx += media_pkt_idx / 8;
            // Bit offset in mask byte
            media_pkt_idx %= 8;
        }
        assert(fec_packet.size() > 0 && "Not fec packets was generated.");
    }
}

void FecEncoder::FinalizeFecHeaders(size_t packet_mask_size, 
                                    uint32_t media_ssrc, 
                                    uint16_t seq_num_base, 
                                    size_t num_fec_packets, 
                                    std::vector<CopyOnWriteBuffer>& generated_fec_packets) {
    for (size_t row = 0; row < num_fec_packets; ++row) {
        fec_header_writer_->FinalizeFecHeader(media_ssrc, 
                                              seq_num_base, 
                                              &packet_masks_[row * packet_mask_size], 
                                              packet_mask_size, 
                                              generated_fec_packets[row]);
    }
}

ssize_t FecEncoder::InsertZeroInPacketMasks(const PacketList& media_packets, size_t num_fec_packets) {
    size_t num_media_packets = media_packets.size();
    // FIXME: Why is not num_media_packets == 0?
    if (num_media_packets <= 1) {
        return num_media_packets;
    }

    uint16_t last_seq_num = media_packets.back().sequence_number();
    uint16_t first_seq_num = media_packets.front().sequence_number();

    // missing packets = logic packets - real packets
    const size_t missing_seq_nums = static_cast<uint16_t>(last_seq_num - first_seq_num + 1) - num_media_packets;

    // No missing media packets,
    // All media packets are convered by the packet mask.
    if (missing_seq_nums == 0) {
        return num_media_packets;
    }

    const size_t max_media_packets = fec_header_writer_->max_media_packets();
    if (missing_seq_nums + num_media_packets > max_media_packets) {
        return -1;
    }

    size_t tmp_packet_mask_size = FecPacketMaskGenerator::PacketMaskSize(missing_seq_nums + num_media_packets);
    memset(tmp_packet_masks_, 0, num_fec_packets * tmp_packet_mask_size);

    size_t new_bit_index = 0;
    size_t old_bit_index = 0;
    // Insert the first column
    internal::CopyColumn(num_fec_packets, packet_mask_size_, tmp_packet_mask_size, old_bit_index, new_bit_index, packet_masks_, tmp_packet_masks_);

    uint16_t prev_seq_num = first_seq_num;
    uint16_t curr_seq_num = prev_seq_num;
    size_t num_zero_to_insert = 0;
    auto media_packet_it = media_packets.cbegin();
    ++media_packet_it;
    ++old_bit_index;
    ++new_bit_index;

    while (media_packet_it != media_packets.end()) {
        if (new_bit_index == max_media_packets) {
            break;
        }
        curr_seq_num = media_packet_it->sequence_number();
        num_zero_to_insert = curr_seq_num - prev_seq_num - 1;

        if (num_zero_to_insert > 0) {
            internal::InsertZeroColumns(num_zero_to_insert, 
                                        num_fec_packets, 
                                        tmp_packet_mask_size, 
                                        new_bit_index, 
                                        tmp_packet_masks_);
        }
        new_bit_index += num_zero_to_insert;
        internal::CopyColumn(num_fec_packets, 
                             packet_mask_size_, 
                             tmp_packet_mask_size, 
                             old_bit_index, 
                             new_bit_index, 
                             packet_masks_, 
                             tmp_packet_masks_);

        ++new_bit_index;
        ++old_bit_index;
        prev_seq_num = curr_seq_num;
        ++media_packet_it;
    }

    if (new_bit_index % 8 != 0) {
        for (size_t row = 0; row < num_fec_packets; ++row) {
            size_t new_byte_index = row * tmp_packet_mask_size + new_bit_index / 8;
            tmp_packet_masks_[new_byte_index] <<= (7 - (new_bit_index % 8));
        }
    }
    memcpy(packet_masks_, tmp_packet_masks_, num_fec_packets * tmp_packet_mask_size);
    return new_bit_index;
}
    
} // namespace naivertc
