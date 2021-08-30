#ifndef _RTC_RTP_RTCP_FEC_DEFINES_H_
#define _RTC_RTP_RTCP_FEC_DEFINES_H_

#include "base/defines.hpp"
#include "common/array_view.hpp"

namespace naivertc {
    
// Packet mask size in bytes (given L bit).
static constexpr size_t kUlpfecPacketMaskSizeLBitClear = 2;
static constexpr size_t kUlpfecPacketMaskSizeLBitSet = 6;

// Ulpfec can protect packet size in bytes (givem L bit)
static constexpr size_t kUlpfecMaxMediaPacketsLBitClear = 2 * 8; // 16
static constexpr size_t kUlpfecMaxMediaPacketsLBitSet = 6 * 8; // 48

// Maximum number of media packets that can be protected by these packet masks.
static constexpr size_t kUlpfecMaxMediaPackets = kUlpfecMaxMediaPacketsLBitSet;

// Maximum number of FEC packets stored inside ForwardErrorCorrection.
static constexpr size_t kMaxFecPackets = kUlpfecMaxMediaPackets;

// Convenience constants.
static constexpr size_t kUlpfecMinPacketMaskSize = kUlpfecPacketMaskSizeLBitClear;
static constexpr size_t kUlpfecMaxPacketMaskSize = kUlpfecPacketMaskSizeLBitSet;

// Packet code mask maximum length. kFECPacketMaskMaxSize = kUlpfecMaxMediaPackets * (kUlpfecMaxMediaPackets / 8),
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

using FecPacket = std::vector<uint8_t>;
    
} // namespace naivertc


#endif