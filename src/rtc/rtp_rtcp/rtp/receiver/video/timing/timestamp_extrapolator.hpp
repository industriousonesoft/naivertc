#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_TIMING_TIMESTAMP_EXTRAPOLATOR_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_TIMING_TIMESTAMP_EXTRAPOLATOR_H_

#include "base/defines.hpp"

#include <optional>

namespace naivertc {
namespace rtp {
namespace video {

// This class is not thread-safety, the caller MUST provide that.
class RTC_CPP_EXPORT TimestampExtrapolator {
public:
    explicit TimestampExtrapolator(int64_t start_time_ms);
    ~TimestampExtrapolator();

    void Update(uint32_t timestamp /* in 90khz */, int64_t receive_time_ms);
    void Reset(int64_t start_time_ms);
    int64_t ExtrapolateLocalTime(uint32_t timestamp /* in 90khz */);

private:
    int64_t Unwrap(uint32_t timestamp);
    bool DelayChangeDetection(double error);
private:
    double w_[2];
    double pP_[2][2];
    int64_t start_time_ms_;
    int64_t prev_time_ms_;
    uint32_t first_timestamp_;
    int32_t num_wrap_arounds_;
    std::optional<int64_t> prev_unwrapped_timestamp_;
    std::optional<uint32_t> prev_wrap_timestamp_;
    bool first_after_reset_;
    uint32_t packet_count_;
    double detector_accumulator_pos_;
    double detector_accumulator_neg_;
};
    
} // namespace video
} // namespace rtp
} // namespace naivert 


#endif