#ifndef _RTC_BASE_RATE_STATISTICS_H_
#define _RTC_BASE_RATE_STATISTICS_H_

#include "base/defines.hpp"
#include "rtc/base/units/bit_rate.hpp"

#include <deque>
#include <optional>

namespace naivertc {

// NOTE: 使用Timestamp无小数转换，不方便计算bit rate，因此使用int64_t替代
// This class is not thread safe, the caller must provide that.
class RTC_CPP_EXPORT BitRateStatistics {
public:
    // We need the max_window_size_ms be specified.
    BitRateStatistics() = delete; 
    BitRateStatistics(const int64_t max_window_size_ms);
    BitRateStatistics(const BitRateStatistics&);
    BitRateStatistics(BitRateStatistics&&);
    ~BitRateStatistics();

    void Reset();

    bool SetWindowSize(int64_t window_size_ms, int64_t now);

    void Update(int64_t bytes, int64_t now_ms);

    std::optional<BitRate> Rate(int64_t now_ms);

#if ENABLE_TESTS
    size_t num_bucket() const { return buckets_.size(); }
    int64_t total_accumulated_bytes() const { return total_accumulated_bytes_; }
    int64_t total_num_samples() const { return total_num_samples_; }
    bool is_overflow() const { return is_overflow_; }
#endif

private:
    void EraseObsoleteBuckets(int64_t now_ms);
private:
    struct Bucket {
        explicit Bucket(const int64_t timestamp);
        ~Bucket();

        // Accumulated bytes recorded in this bucket.
        int64_t accumulated_bytes;
        // Number of samples in this bucket.
        size_t num_samples;
        // The timestamp this bucket corresponds to.
        const int64_t timestamp;
        // True is the accumulated_bytes of the bucket is not counted to total_accumulated_bytes_
        bool is_overflow;
    };
private:
    std::deque<Bucket> buckets_;

    int64_t total_accumulated_bytes_;

    size_t total_num_samples_;

    int64_t begin_timestamp_ms_;

    // True is total_accumulated_bytes_ has ever grown too larger to 
    // be greater than the max value in its integer type.
    bool is_overflow_;

    // The window sizes, in ms, over which the rate is calculated.
    const int64_t max_window_size_ms_;
    int64_t current_window_size_ms_;

};

} // namespace naivertc

#endif