#ifndef _RTC_RTP_RTCP_FEC_DEFINES_H_
#define _RTC_RTP_RTCP_FEC_DEFINES_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"

namespace naivertc {

// Packet mask size in bytes (given L bit).
static constexpr size_t kUlpFecPacketMaskSizeLBitClear = 2;
static constexpr size_t kUlpFecPacketMaskSizeLBitSet = 6;

// FEC Level 0 header size in bytes.
static constexpr size_t kFecLevel0HeaderSize = 10;
// FEC Level 1 (ULP) header size in bytes (L bit is set).
static constexpr size_t kFecLevel1HeaderSizeLBitSet = 2 + kUlpFecPacketMaskSizeLBitSet;
// FEC Level 1 (ULP) header size in bytes (L bit is cleared).
static constexpr size_t kFecLevel1HeaderSizeLBitClear = 2 + kUlpFecPacketMaskSizeLBitClear;
    
// UlpFec can protect packet size in bytes (givem L bit)
static constexpr size_t kUlpFecMaxMediaPacketsLBitClear = 2 * 8; // 16
static constexpr size_t kUlpFecMaxMediaPacketsLBitSet = 6 * 8; // 48

// Maximum number of media packets that can be protected by these packet masks.
static constexpr size_t kUlpFecMaxMediaPackets = kUlpFecMaxMediaPacketsLBitSet;

// Maximum number of FEC packets stored inside ForwardErrorCorrection.
// TODO: Why we need to limit the FEC packet count with a fixed number here?
// and we can do the same thing based on a reasonable age of sequence number.
static constexpr size_t kMaxFecPackets = kUlpFecMaxMediaPackets;

// Convenience constants.
static constexpr size_t kUlpFecMinPacketMaskSize = kUlpFecPacketMaskSizeLBitClear;
static constexpr size_t kUlpFecMaxPacketMaskSize = kUlpFecPacketMaskSizeLBitSet;

// Packet code mask maximum length. kFECPacketMaskMaxSize = kUlpFecMaxMediaPackets * (kUlpFecMaxMediaPackets / 8),
static constexpr size_t kFECPacketMaskMaxSize = 288;

// Types for the FEC packet masks. The type |kFecMaskRandom| is based on a
// random loss model. The type bursty is based on a bursty/consecutive
// loss model.
enum class FecMaskType {
    RANDOM,
    BURSTY,
};

// Struct containing forward error correction settings.
struct RTC_CPP_EXPORT FecProtectionParams {
    size_t fec_rate = 0;
    size_t max_fec_frames = 0;
    FecMaskType fec_mask_type = FecMaskType::RANDOM;
};

// FEC Header
struct RTC_CPP_EXPORT FecHeader {
    // FEC header fields
    size_t fec_header_size = 0;
    uint16_t seq_num_base = 0;
    // Relative start of FEC header.
    size_t packet_mask_offset = 0;
    size_t packet_mask_size = 0;
    // TODO: Remove `protection length` field maybe.
    // `protection length` field is the same thing with 
    // `length recovery` in WebRTC implement.
    size_t protection_length = 0;
};
    
} // namespace naivertc


#endif