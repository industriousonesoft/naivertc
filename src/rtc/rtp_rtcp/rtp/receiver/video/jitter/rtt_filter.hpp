#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_RTT_FILTER_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_JITTER_RTT_FILTER_H_

#include "base/defines.hpp"

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {

// RttFilter stores a periodic rtt valus to detect if a rtt jump or drift happens or not,
// and calculates the current rtt.
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
    uint8_t sample_count_;

    // The size of the drift and jump memory buffers
    // and thus also the detection threshold for these
    // detectors in number of samples.
    static const size_t kDetectThreshold = 5;
    int64_t jump_buffer_[kDetectThreshold] = {0};
    int64_t drift_buffer_[kDetectThreshold] = {0};
};
    
} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivertc


#endif