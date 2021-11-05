#include "rtc/rtp_rtcp/rtp/receiver/video/timing/decode_time_filter.hpp"

namespace naivertc {
namespace rtp {
namespace video {

// Sample
DecodeTimeFilter::Sample::Sample(int64_t decode_time_ms, int64_t sample_time_ms) 
    : decode_time_ms(decode_time_ms),
      sample_time_ms(sample_time_ms) {}

// DecodeTimeFilter
DecodeTimeFilter::DecodeTimeFilter(float percentile, 
                                   int64_t time_window_size_ms, 
                                   size_t ignored_sample_threshold) 
    : window_size_ms_(time_window_size_ms),
      ignored_sample_threshold_(ignored_sample_threshold),
      ignored_sample_count_(0), 
      filter_(percentile) {}

DecodeTimeFilter::~DecodeTimeFilter() {}

void DecodeTimeFilter::AddTiming(int64_t decode_time_ms, int64_t now_ms) {
    // Ignore the first `ignored_sample_threshold_` samples.
    if (ignored_sample_count_ < ignored_sample_threshold_) {
        ++ignored_sample_count_;
        return;
    }

    // Insert new decode time value.
    filter_.Insert(decode_time_ms);
    history_.emplace(decode_time_ms, now_ms);

    // Remove the old samples.
    while (!history_.empty() && now_ms - history_.front().sample_time_ms > window_size_ms_) {
        filter_.Erase(history_.front().decode_time_ms);
        history_.pop();
    }
}

int64_t DecodeTimeFilter::RequiredDecodeTimeMs() const {
    return filter_.GetPercentileValue();
}

void DecodeTimeFilter::Reset() {
    ignored_sample_count_ = 0;
    std::queue<Sample>().swap(history_);
    filter_.Reset();
}

} // namespace video    
} // namespace rtp
} // namespace naivertc