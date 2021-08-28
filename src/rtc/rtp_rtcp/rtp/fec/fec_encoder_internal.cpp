#include "rtc/rtp_rtcp/rtp/fec/fec_encoder.hpp"

#include <plog/Log.h>

namespace naivertc {

FecEncoder::PacketMaskTable::PacketMaskTable(FecMaskType fec_mask_type, size_t num_media_packets) 
    : table_(PickTable(fec_mask_type, num_media_packets)) {}

FecEncoder::PacketMaskTable::~PacketMaskTable() = default;

ArrayView<const uint8_t> FecEncoder::PacketMaskTable::LookUp(size_t num_media_packets, size_t num_fec_packets) {

    if (num_media_packets > kUlpfecMaxMediaPackets || num_media_packets < num_fec_packets) {
        PLOG_WARNING << "Invalid parameters, num_media_packets: " 
                     << num_media_packets << ", num_fec_packets: " 
                     << num_fec_packets << ".";
        return nullptr;
    }

    if (num_media_packets <= table_[0]) {
        return LookUpInFecTable(table_, num_media_packets - 1, num_fec_packets - 1);
    }

    // Starting from 13 media packets, the fec code will be generated at runtime.

    size_t mask_size = PacketMaskSize(num_media_packets);

    // Generate FEC code mask for {num_media_packets(M), num_fec_packets(N)} (use
    // N FEC packets to protect M media packets) In the mask, each FEC packet
    // occupies one row, each bit / coloumn represent one media packet. E.g. Row
    // A, Col/Bit B is set to 1, means FEC packet A will have protection for media
    // packet B.

    for (size_t row = 0; row < num_fec_packets; row++) {
        // Loop through each fec code in a row, one code has 8 bits.
        // Bit X will be set to 1 if media packet X shall be protected by current
        // FEC packet. 
        // In this implementation, the protection is interleaved, thus
        // media packet X will be protected by FEC packet (X % N)
        // TODO: Other implementations?
        for (size_t col = 0; col < mask_size; col++) {
            fec_packet_mask_[row * mask_size + col] = 
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

    return ArrayView<const uint8_t>(&fec_packet_mask_[0], num_fec_packets * mask_size);
}

const uint8_t* FecEncoder::PacketMaskTable::PickTable(FecMaskType fec_mask_type, size_t num_media_packets) {
    assert(num_media_packets <= kUlpfecMaxMediaPackets);

    // The bursty table is explicitly asked and the number of media packets is not larger than 
    // the size of packet mask bursty table.
    if (fec_mask_type != FecMaskType::RANDOM && num_media_packets <= kPacketMaskBurstyTable[0] /* table size*/) {
        return kPacketMaskBurstyTable;
    }

    // Otherwise the random table is returned.
    return kPacketMaskRandomTable;
}

bool FecEncoder::PacketMaskTable::GeneratePacketMasks(size_t num_media_packets,
                                                      size_t num_fec_packets,
                                                      size_t num_important_packets,
                                                      bool use_unequal_protection,
                                                      uint8_t* packet_mask) {
    if (num_fec_packets > num_media_packets) {
        return false;
    }
    if (num_important_packets > num_media_packets) {
        return false;
    }

    const size_t num_mask_bytes = PacketMaskSize(num_media_packets);

    // Packet masks in equal protection
    if (!use_unequal_protection || num_important_packets == 0) {
        // Mask = (k,n-k), with protection factor = (n-k)/k,
        // where k = num_media_packets, n=total#packets, (n-k)=num_fec_packets.
        ArrayView<const uint8_t> masks = LookUp(num_media_packets, num_fec_packets);
        memcpy(packet_mask, masks.data(), masks.size());
    }else {
        // TODO: generate packet mask in unequal protection
    }
    return true;
}

// Static methods
ArrayView<const uint8_t> FecEncoder::LookUpInFecTable(const uint8_t* table, size_t media_packet_index, size_t fec_packet_index) {
    if (media_packet_index >= table[0]) {
        return nullptr;
    }

    const uint8_t* entry = &table[1];

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

size_t FecEncoder::PacketMaskSize(size_t num_packets) {
    // The number of packets MUST be lower than 48.
    assert(num_packets <= kUlpfecMaxMediaPackets);
    if (num_packets > kUlpfecMaxMediaPacketsLBitClear) {
        return kUlpfecPacketMaskSizeLBitSet;
    }
    return kUlpfecPacketMaskSizeLBitClear;
}
    
} // namespace naivertc
