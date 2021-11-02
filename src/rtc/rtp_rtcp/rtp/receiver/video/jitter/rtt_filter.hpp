#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_RTT_FILTER_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_RTT_FILTER_H_

#include "base/defines.hpp"

namespace naivert {
namespace rtc {
namespace video {
namespace jitter {

class RTC_CPP_EXPORT RttFilter {
public:
    RttFilter();
    RttFilter& operator=(const RttFilter& rhs);

    // Add a new rtt sample to the filter.
    void AddRtt(int64_t rtt_ms);

    // Returns the current RTT in ms.
    int64_t RttMs() const;

    // Reset the filter.
    void Reset();
    
private:
    bool JumpDetection(int64_t rtt_ms, int64_t avg_rtt, int64_t var_rtt);
    bool DriftDetection(int64_t rtt_ms, int64_t avg_rtt, int64_t var_rtt, int64_t max_rtt);
    void UpdateRtts(int64_t* buf, uint8_t count);

private:
    bool has_first_non_zero_update_;
    double avg_rtt_;
    double var_rtt_;
    int64_t max_rtt_;
    int8_t jump_count_;
    int8_t drift_count_;
    uint8_t filt_fact_count_;
    const uint8_t filt_fact_max_;
    const double jump_std_devs_;
    const double drift_std_devs_;
    const int8_t detect_threshold_;

    // The size of the drift and jump memory buffers
    // and thus also the detection threshold for these
    // detectors in number of samples.
    static const size_t kMaxDriftJumpCount = 5;
    int64_t jump_buffer_[kMaxDriftJumpCount] = {0};
    int64_t drift_buffer_[kMaxDriftJumpCount] = {0};
};
    
} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivert


#endif