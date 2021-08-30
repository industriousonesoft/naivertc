#include "rtc/rtp_rtcp/rtp/fec/fec_mask_generator.hpp"

#include <plog/Log.h>

namespace naivertc {

FecPacketMaskGenerator::FecPacketMaskGenerator() 
    : fixed_mask_table_(nullptr) {
    
}

FecPacketMaskGenerator::~FecPacketMaskGenerator() = default;

bool FecPacketMaskGenerator::GeneratePacketMasks(FecMaskType fec_mask_type,
                                                 size_t num_media_packets,
                                                 size_t num_fec_packets,
                                                 size_t num_imp_packets,
                                                 bool use_unequal_protection,
                                                 uint8_t* packet_masks) {
    if (num_fec_packets > num_media_packets) {
        return false;
    }
    if (num_imp_packets > num_media_packets) {
        return false;
    }

    PickFixedMaskTable(fec_mask_type, num_media_packets);
    // Packet masks in equal protection
    if (!use_unequal_protection || num_imp_packets == 0) {
        // Mask = (k,n-k), with protection factor = (n-k)/k,
        // where k = num_media_packets, n=total#packets, (n-k)=num_fec_packets.
        ArrayView<const uint8_t> masks = LookUpPacketMasks(num_media_packets, num_fec_packets);
        memcpy(packet_masks, masks.data(), masks.size());
    }else {
        const size_t num_mask_bytes = PacketMaskSize(num_media_packets);
        GenerateUnequalProtectionMasks(num_media_packets, 
                                       num_fec_packets, 
                                       num_imp_packets,
                                       num_mask_bytes,
                                       packet_masks);
    }
    return true;
}

// Private methods
void FecPacketMaskGenerator::PickFixedMaskTable(FecMaskType fec_mask_type, size_t num_media_packets) {
    assert(num_media_packets <= kUlpfecMaxMediaPackets);
    // The bursty table is explicitly asked and the number of media packets is not larger than 
    // the size of packet mask bursty table.
    if (fec_mask_type != FecMaskType::RANDOM && num_media_packets <= kPacketMaskBurstyTable[0] /* table size*/) {
        fixed_mask_table_ = kPacketMaskBurstyTable;
    }else {
        // Otherwise the random table is returned.
        fixed_mask_table_ = kPacketMaskRandomTable;
    }
}

void FecPacketMaskGenerator::GenerateUnequalProtectionMasks(size_t num_media_packets,
                                                            size_t num_fec_packets,
                                                            size_t num_imp_packets,
                                                            size_t num_mask_bytes,
                                                            uint8_t* packet_masks,
                                                            UEPMode mode) {
    size_t num_fec_for_imp_packets = 0;
    if (mode != UEPMode::BIAS_FIRST_PACKET) {
        num_fec_for_imp_packets = NumberOfFecPacketForImportantPackets(num_media_packets, 
                                                                       num_fec_packets, 
                                                                       num_imp_packets);
    }

    size_t num_fec_remaining = num_fec_packets - num_fec_for_imp_packets;

    if (num_fec_for_imp_packets > 0) {
        GenerateImportantProtectionMasks(num_fec_for_imp_packets, 
                                         num_imp_packets, 
                                         num_mask_bytes, 
                                         packet_masks);
    }

    if (num_fec_remaining > 0) {
        GenerateRemainingProtectionMasks(num_media_packets, 
                                         num_fec_remaining, 
                                         num_fec_for_imp_packets, 
                                         num_mask_bytes, 
                                         mode, 
                                         packet_masks);
    }
}

size_t FecPacketMaskGenerator::NumberOfFecPacketForImportantPackets(size_t num_media_packets,
                                                                    size_t num_fec_packets,
                                                                    size_t num_imp_packets) {
    // FIXME: 此处为什么是0.5？
    size_t max_num_fec_for_imp = num_fec_packets * 0.5;

    size_t num_fec_for_imp = num_imp_packets < max_num_fec_for_imp ? num_imp_packets : max_num_fec_for_imp;

    // Fall back to equal protection
    // FIXME：怎么理解这个回滚条件？
    if (num_fec_packets == 1 && (num_media_packets > 2 * num_imp_packets)) {
        num_fec_for_imp = 0;
    }

    return num_fec_for_imp;
}

void FecPacketMaskGenerator::GenerateImportantProtectionMasks(size_t num_fec_for_imp_packets,
                                                              size_t num_imp_packets,
                                                              size_t num_mask_bytes,
                                                              uint8_t* packet_masks) {
    const size_t num_imp_mask_bytes = PacketMaskSize(num_imp_packets);

    // Packet masks for important media packets
    ArrayView<const uint8_t> packet_sub_masks = LookUpPacketMasks(num_imp_packets, num_fec_for_imp_packets);

    FitSubMasks(num_mask_bytes, num_imp_mask_bytes, num_fec_for_imp_packets, packet_sub_masks.data(), packet_masks);
}

void FecPacketMaskGenerator::GenerateRemainingProtectionMasks(size_t num_media_packets,
                                                              size_t num_fec_remaining,
                                                              size_t num_fec_for_imp_packets,
                                                              size_t num_mask_bytes,
                                                              UEPMode mode,
                                                              uint8_t* packet_masks) {
    if (mode == UEPMode::OVERLAP || mode == UEPMode::BIAS_FIRST_PACKET) {
        // Overlap and bias-first-packet protection mode will protect the imaport packets with remaining FEC packets.
        ArrayView<const uint8_t> packet_sub_masks = LookUpPacketMasks(num_media_packets, num_fec_remaining);
        FitSubMasks(num_mask_bytes, 
                    num_mask_bytes, 
                    num_fec_remaining, 
                    packet_sub_masks.data(), 
                    &packet_masks[num_fec_for_imp_packets * num_mask_bytes]);
        // bias-first-packet protection:
        // All the remaining FEC packts will protect the first packet, 
        // which means the first byte in every packet mask must be greater than 0x80 
        if (mode == UEPMode::BIAS_FIRST_PACKET) {
            for (size_t i = 0; i < num_fec_remaining; ++i) {
                packet_masks[i * num_mask_bytes] |= 0x80; // 1 << 7
            }
        }
    }else if (mode == UEPMode::NO_OVERLAP) {
        // FIXME: 此处为什么是减去num_fec_for_imp_packets而非num_for_imp_packets？？
        const size_t num_media_packets_remaining = num_media_packets - num_fec_for_imp_packets;

        const size_t num_sub_mask_bytes = PacketMaskSize(num_media_packets_remaining);

        size_t end_row = num_fec_for_imp_packets + num_fec_remaining;

        ArrayView<const uint8_t> packet_sub_masks = LookUpPacketMasks(num_media_packets_remaining, num_fec_remaining);

        ShiftFitSubMask(num_mask_bytes, num_sub_mask_bytes, num_fec_for_imp_packets, end_row, packet_sub_masks.data(), packet_masks);
        
    }else {
        RTC_NOTREACHED();
    }

}

size_t FecPacketMaskGenerator::PacketMaskSize(size_t num_packets) {
    // The number of packets MUST be lower than 48.
    assert(num_packets <= kUlpfecMaxMediaPackets);
    if (num_packets > kUlpfecMaxMediaPacketsLBitClear) {
        return kUlpfecPacketMaskSizeLBitSet;
    }
    return kUlpfecPacketMaskSizeLBitClear;
}

void FecPacketMaskGenerator::FitSubMasks(size_t num_mask_stride, 
                                         size_t num_sub_mask_stride, 
                                         size_t num_row, 
                                         const uint8_t* sub_packet_masks, 
                                         uint8_t* packet_masks) {
    assert(num_sub_mask_stride <= num_mask_stride);
    // In the same stride
    if (num_mask_stride == num_sub_mask_stride) {
        memcpy(packet_masks, sub_packet_masks, num_row * num_sub_mask_stride);
    }else {
        // Transform 1-D array to 2-D array in a stride
        for (size_t row = 0; row < num_row; ++row) {
            size_t dst_col_begin = num_mask_stride * row;
            size_t src_col_begin = num_sub_mask_stride * row;
            for (size_t col = 0; col < num_sub_mask_stride; ++col) {
                packet_masks[dst_col_begin + col] = sub_packet_masks[src_col_begin + col];
            }
        }
    }
}

void FecPacketMaskGenerator::ShiftFitSubMask(size_t num_mask_bytes, 
                                             size_t num_sub_mask_bytes, 
                                             size_t num_col_shift, 
                                             size_t end_row, 
                                             const uint8_t* sub_packet_masks, 
                                             uint8_t* packet_masks) {
    // TODO: Shift and fit packet sub masks.
    // modules/rtp_rtcp/source/forward_error_correction_internal.cc:80
}

ArrayView<const uint8_t> FecPacketMaskGenerator::LookUpInFixedMaskTable(const uint8_t* mask_table, 
                                                                        size_t media_packet_index, 
                                                                        size_t fec_packet_index) {
    if (media_packet_index >= mask_table[0]/* mask table size */) {
        return nullptr;
    }

    const uint8_t* entry = &mask_table[1];

    // 0 - 16 are 2 byte wide, than changes to 6.
    uint8_t entry_size_increment = kUlpfecPacketMaskSizeLBitClear; // 2

    // Hop over un-interesting array entries.
    for (size_t i = 0; i < media_packet_index; ++i) {
        if (i == kUlpfecMaxMediaPacketsLBitClear) {
            entry_size_increment = kUlpfecPacketMaskSizeLBitSet; // 6
        }
        // Entry item count in the first byte 
        uint8_t entry_item_count = entry[0];
        // Skip over the count byte
        ++entry;
        for (size_t j = 0; j < entry_item_count; ++j) {
            // Skip over the data
            entry += entry_size_increment * (j + 1);
        }
    }

    if (media_packet_index == kUlpfecMaxMediaPacketsLBitClear) {
        entry_size_increment = kUlpfecPacketMaskSizeLBitSet;
    }

    if (fec_packet_index >= entry[0]) {
        return nullptr;
    }

    // Skip over the count byte
    ++entry;

    for (size_t i = 0; i < fec_packet_index; ++i) {
        // Skip over the data
        entry += entry_size_increment * (i + 1);
    }

    size_t size = entry_size_increment * (fec_packet_index + 1);
    return ArrayView<const uint8_t>(&entry[0], size);
}

ArrayView<const uint8_t> FecPacketMaskGenerator::LookUpPacketMasks(size_t num_media_packets, size_t num_fec_packets) {

    if (num_media_packets > kUlpfecMaxMediaPackets || num_media_packets < num_fec_packets) {
        PLOG_WARNING << "Invalid parameters, num_media_packets: " 
                     << num_media_packets << ", num_fec_packets: " 
                     << num_fec_packets << ".";
        return nullptr;
    }

    if (num_media_packets <= fixed_mask_table_[0]) {
        return LookUpInFixedMaskTable(fixed_mask_table_ ,num_media_packets - 1, num_fec_packets - 1);
    }

    // Starting from 13 media packets, the fec code will be generated at runtime.

    size_t mask_size = PacketMaskSize(num_media_packets);

    // Generate FEC code mask for {num_media_packets(M), num_fec_packets(N)} (use
    // N FEC packets to protect M media packets) In the mask, each FEC packet
    // occupies one row, each bit / coloumn represent one media packet. E.g. Row
    // A, Col/Bit B is set to 1, means FEC packet A will have protection for media
    // packet B.

    memset(fec_packet_masks_, 0, kFECPacketMaskMaxSize);
    for (size_t row = 0; row < num_fec_packets; row++) {
        // Loop through each fec code in a row, one code has 8 bits.
        // Bit X will be set to 1 if media packet X shall be protected by current
        // FEC packet. 
        // In this implementation, the protection is interleaved, thus
        // media packet X will be protected by FEC packet (X % N)
        // TODO: Other implementations?
        for (size_t col = 0; col < mask_size; col++) {
            fec_packet_masks_[row * mask_size + col] = 
            ((col * 8) % num_fec_packets == row && (col * 8) < num_media_packets
               ? 0x80
               : 0x00) |
            ((col * 8 + 1) % num_fec_packets == row &&
                    (col * 8 + 1) < num_media_packets
                ? 0x40
                : 0x00) |
            ((col * 8 + 2) % num_fec_packets == row &&
                    (col * 8 + 2) < num_media_packets
                ? 0x20
                : 0x00) |
            ((col * 8 + 3) % num_fec_packets == row &&
                    (col * 8 + 3) < num_media_packets
                ? 0x10
                : 0x00) |
            ((col * 8 + 4) % num_fec_packets == row &&
                    (col * 8 + 4) < num_media_packets
                ? 0x08
                : 0x00) |
            ((col * 8 + 5) % num_fec_packets == row &&
                    (col * 8 + 5) < num_media_packets
                ? 0x04
                : 0x00) |
            ((col * 8 + 6) % num_fec_packets == row &&
                    (col * 8 + 6) < num_media_packets
                ? 0x02
                : 0x00) |
            ((col * 8 + 7) % num_fec_packets == row &&
                    (col * 8 + 7) < num_media_packets
                ? 0x01
                : 0x00);
        }
    }

    return ArrayView<const uint8_t>(&fec_packet_masks_[0], num_fec_packets * mask_size);
}
    
} // namespace naivertc
