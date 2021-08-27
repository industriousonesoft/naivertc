#ifndef _RTC_RTP_RTCP_FEC_DEFINES_H_
#define _RTC_RTP_RTCP_FEC_DEFINES_H_

#include "base/defines.hpp"

namespace naivertc {

// Types for the FEC packet masks. The type |kFecMaskRandom| is based on a
// random loss model. The type bursty is based on a bursty/consecutive
// loss model.
enum FecMaskType {
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