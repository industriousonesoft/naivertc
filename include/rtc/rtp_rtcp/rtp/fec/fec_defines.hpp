#ifndef _RTC_RTP_RTCP_FEC_DEFINES_H_
#define _RTC_RTP_RTCP_FEC_DEFINES_H_

#include "base/defines.hpp"

namespace naivertc {

// Maximum number of media packets that can be protected by these packet masks.
constexpr size_t kUlpfecMaxMediaPackets = 48;

enum class PacketMaskBitIndicator {
    CLEAR,
    SET
};

// Packet mask size in bytes (given L bit).
constexpr size_t kUlpfecPacketMaskSizeLBitClear = 2;
constexpr size_t kUlpfecPacketMaskSizeLBitSet = 6;

// Convenience constants.
constexpr size_t kUlpfecMinPacketMaskSize = kUlpfecPacketMaskSizeLBitClear;
constexpr size_t kUlpfecMaxPacketMaskSize = kUlpfecPacketMaskSizeLBitSet;

// Packet code mask maximum length. kFECPacketMaskMaxSize = MaxNumFECPackets *
// (kUlpfecMaxMediaPackets / 8), and MaxNumFECPackets is equal to maximum number
// of media packets (kUlpfecMaxMediaPackets)
constexpr size_t kFECPacketMaskMaxSize = 288;

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
    
} // namespace naivertc


#endif