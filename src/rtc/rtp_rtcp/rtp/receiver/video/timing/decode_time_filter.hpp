#ifndef _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_TIMING_DECODE_TIME_FILTER_H_
#define _RTC_RTP_RTCP_RTP_RECEIVER_VIDEO_TIMING_DECODE_TIME_FILTER_H_

#include "base/defines.hpp"
#include "rtc/base/numerics/percentile_filter.hpp"

#include <queue>

namespace naivertc {
namespace rtp {
namespace video {
namespace {

// Return the |kPercentile| value in RequiredDecodeTimeMs().
constexpr float kDefaultPercentile = 0.95f;
// The window size in ms.
constexpr int64_t kDefaultTimeWindowSizeMs = 10000; // 10s
// Ignore the first threshold samples.
constexpr int kDefaultIgnoredSampleThreshold = 5;

}  // namespace

class DecodeTimeFilter {
public:
    // `percentile` should between 0 ~ 1.
    DecodeTimeFilter(float percentile = kDefaultPercentile, 
                     int64_t time_window_size_ms = kDefaultTimeWindowSizeMs, 
                     size_t ignored_sample_threshold = kDefaultIgnoredSampleThreshold);
    ~DecodeTimeFilter();

    void AddTiming(int64_t decode_time_ms, int64_t now_ms);
    
    // Get the required decode time in ms. It is the 95th percentile observed
    // decode time within a time window.
    int64_t RequiredDecodeTimeMs() const;

    void Reset();

private:
    struct Sample {
        Sample(int64_t decode_time_ms, int64_t sample_time_ms);
        int64_t decode_time_ms;
        int64_t sample_time_ms;
    };
private:
    const int64_t window_size_ms_;
    const size_t ignored_sample_threshold_;
    int ignored_sample_count_;
    std::queue<Sample> history_;
    PercentileFilter<int64_t> filter_;
};
    
} // namespace video    
} // namespace rtp
} // namespace naivertc


#endif