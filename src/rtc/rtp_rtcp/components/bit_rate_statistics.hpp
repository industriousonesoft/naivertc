#ifndef _RTC_RTP_RTCP_COMPONENTS_RATE_STATISTICS_H_
#define _RTC_RTP_RTCP_COMPONENTS_RATE_STATISTICS_H_

#include "base/defines.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/units/time_delta.hpp"

#include <deque>
#include <optional>

namespace naivertc {

// Class to estimate bitrates based on bytes in a sequence of 1-millisecond
// intervals.
// This class is not thread safe, the caller must provide that.
class BitrateStatistics {
public:
    static constexpr TimeDelta kDefauleWindowSize = TimeDelta::Seconds(1); // 1s
public:
    // We need the max_window_size be specified.
    BitrateStatistics(TimeDelta max_window_size = kDefauleWindowSize);
    BitrateStatistics(const BitrateStatistics&);
    BitrateStatistics(BitrateStatistics&&);
    ~BitrateStatistics();

    void Reset();

    bool SetWindowSize(TimeDelta window_size_ms, Timestamp at_time);

    void Update(int64_t bytes, Timestamp at_time);

    std::optional<DataRate> Rate(Timestamp at_time) const;

    // For unit tests
    size_t num_bucket() const { return buckets_.size(); }
    int64_t accumulated_bytes() const { return accumulated_bytes_; }
    int64_t num_samples() const { return num_samples_; }
    bool is_overflowed() const { return is_overflowed_; }

private:
    void EraseOld(Timestamp at_time);
private:
    struct Bucket {
        explicit Bucket(Timestamp timestamp);
        ~Bucket();

        // Accumulated bytes recorded in this bucket.
        int64_t accumulated_bytes;
        // Number of samples in this bucket.
        size_t num_samples;
        // The timestamp this bucket corresponds to.
        const Timestamp timestamp;
    };
private:
    std::deque<Bucket> buckets_;

    int64_t accumulated_bytes_;

    size_t num_samples_;

    std::optional<Timestamp> first_update_time_;

    // True is accumulated_bytes_ has ever grown too larger to 
    // be greater than the max value in its integer type.
    bool is_overflowed_;

    // The window sizes, in ms, over which the rate is calculated.
    const TimeDelta max_window_size_;
    TimeDelta current_window_size_;

};

} // namespace naivertc

#endif