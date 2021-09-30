#include "rtc/rtp_rtcp/components/bit_rate_statistics.hpp"

#include <limits>

namespace naivertc {

// The window size of a single bucket is 1ms
constexpr int64_t kSingleBucketWindowSizeMs = 1;
constexpr int64_t kInvalidTimestamp = -1;

// Bucket
BitRateStatistics::Bucket::Bucket(const int64_t timestamp) 
    : accumulated_bytes(0),
      num_samples(0),
      timestamp(std::move(timestamp)),
      is_overflow(false) {}

BitRateStatistics::Bucket::~Bucket() = default;

// BitRateStatistics
BitRateStatistics::BitRateStatistics(const int64_t max_window_size_ms) 
    : total_accumulated_bytes_(0),
      total_num_samples_(0),
      begin_timestamp_ms_(kInvalidTimestamp),
      is_overflow_(false),
      max_window_size_ms_(max_window_size_ms),
      current_window_size_ms_(max_window_size_ms_) {}

BitRateStatistics::BitRateStatistics(const BitRateStatistics& other) 
    : buckets_(other.buckets_),
      total_accumulated_bytes_(other.total_accumulated_bytes_),
      total_num_samples_(other.total_num_samples_),
      begin_timestamp_ms_(other.begin_timestamp_ms_),
      is_overflow_(other.is_overflow_),
      max_window_size_ms_(other.max_window_size_ms_),
      current_window_size_ms_(other.current_window_size_ms_) {}

BitRateStatistics::BitRateStatistics(BitRateStatistics&& other) = default;

BitRateStatistics::~BitRateStatistics() {}

void BitRateStatistics::Reset() {
    total_accumulated_bytes_ = 0;
    total_num_samples_ = 0;
    begin_timestamp_ms_ = kInvalidTimestamp;
    is_overflow_ = false;
    current_window_size_ms_ = max_window_size_ms_;
    buckets_.clear();
}

void BitRateStatistics::Update(int64_t bytes, int64_t now_ms) {
    EraseObsoleteBuckets(now_ms);

    if (begin_timestamp_ms_ == kInvalidTimestamp) {
        begin_timestamp_ms_ = now_ms;
    }

    if (buckets_.empty() || now_ms > buckets_.back().timestamp) {
        buckets_.emplace_back(now_ms);
    }
    Bucket& last_bucket = buckets_.back();
    last_bucket.accumulated_bytes += bytes;
    last_bucket.num_samples += 1;
    
    if (std::numeric_limits<int64_t>::max() > total_accumulated_bytes_ + bytes) {
        total_accumulated_bytes_ += bytes;
        last_bucket.is_overflow = false;
    }else {
        is_overflow_ = true;
        last_bucket.is_overflow = true;
    }
    ++total_num_samples_;
}

std::optional<BitRate> BitRateStatistics::Rate(int64_t now_ms) {
    // Erase the obsolete buckets
    EraseObsoleteBuckets(now_ms);

    // The active window size should be a single bucket window size at least.
    int64_t active_window_size_ms = 0;
    if (begin_timestamp_ms_ != kInvalidTimestamp && !is_overflow_) {
        if (begin_timestamp_ms_ >= now_ms) {
            return std::nullopt;
        }else if (now_ms - begin_timestamp_ms_ >= current_window_size_ms_) {
            active_window_size_ms = current_window_size_ms_;
        }else {
            active_window_size_ms = now_ms - begin_timestamp_ms_ + kSingleBucketWindowSizeMs;
            // Only one single samples and not full window size are not enough for valid estimate.
            if (total_num_samples_ == 1 && active_window_size_ms < current_window_size_ms_) {
                return std::nullopt;
            } 
        }
    }else {
        // No sample in buckets or the accumulator has overflowed.
        return std::nullopt;
    }
    float bits_per_sec = total_accumulated_bytes_ * 8000.0 / active_window_size_ms + 0.5f;
    // Better return unavailable rate than garbage value (undefined behavior).
    if (bits_per_sec < 0 /* overflow */ || bits_per_sec > static_cast<float>(std::numeric_limits<int64_t>::max())) {
        return std::nullopt;
    }
    return BitRate::BitsPerSec(static_cast<int64_t>(bits_per_sec));
}

bool BitRateStatistics::SetWindowSize(int64_t window_size_ms, int64_t now_ms) {
    if (window_size_ms == 0 || window_size_ms > max_window_size_ms_) {
        return false;
    }
    if (begin_timestamp_ms_ != kInvalidTimestamp && now_ms - begin_timestamp_ms_ > window_size_ms) {
        begin_timestamp_ms_ = now_ms - window_size_ms + kSingleBucketWindowSizeMs;
    }
    current_window_size_ms_ = window_size_ms;
    EraseObsoleteBuckets(now_ms);
    return true;
}

// Private methods
void BitRateStatistics::EraseObsoleteBuckets(int64_t now_ms) {
    // The result maybe lower than begin_timestamp_ms_ or a negative value, it still works.
    const int64_t obsolete_timestamp = now_ms - current_window_size_ms_;
    while (!buckets_.empty() && buckets_.front().timestamp <= obsolete_timestamp) {
        const Bucket& obsolete_bucket = buckets_.front();
        if (!obsolete_bucket.is_overflow) {
            total_accumulated_bytes_ -= obsolete_bucket.accumulated_bytes;
        }
        total_num_samples_ -= obsolete_bucket.num_samples;
        buckets_.pop_front();
        // Reset is_overflow_ when having obsolete buckets be removed
        if (is_overflow_ && total_accumulated_bytes_ < std::numeric_limits<int64_t>::max()) {
            is_overflow_ = false;
        }
    }
    
}
    
} // namespace naivertc
