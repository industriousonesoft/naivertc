#include "rtc/rtp_rtcp/components/bit_rate_statistics.hpp"

#include <plog/Log.h>

#include <limits>

namespace naivertc {

// The window size of a single bucket is 1ms
constexpr TimeDelta kSingleBucketWindowSize = TimeDelta::Millis(1);

// Bucket
BitrateStatistics::Bucket::Bucket(Timestamp timestamp) 
    : accumulated_bytes(0),
      num_samples(0),
      timestamp(std::move(timestamp)) {}

BitrateStatistics::Bucket::~Bucket() = default;

// BitrateStatistics
BitrateStatistics::BitrateStatistics(TimeDelta max_window_size) 
    : accumulated_bytes_(0),
      num_samples_(0),
      first_update_time_(std::nullopt),
      is_overflowed_(false),
      max_window_size_(max_window_size),
      current_window_size_(max_window_size_) {}

BitrateStatistics::BitrateStatistics(const BitrateStatistics& other) 
    : buckets_(other.buckets_),
      accumulated_bytes_(other.accumulated_bytes_),
      num_samples_(other.num_samples_),
      first_update_time_(other.first_update_time_),
      is_overflowed_(other.is_overflowed_),
      max_window_size_(other.max_window_size_),
      current_window_size_(other.current_window_size_) {}

BitrateStatistics::BitrateStatistics(BitrateStatistics&& other) = default;

BitrateStatistics::~BitrateStatistics() {}

void BitrateStatistics::Reset() {
    accumulated_bytes_ = 0;
    num_samples_ = 0;
    first_update_time_ = std::nullopt;
    is_overflowed_ = false;
    current_window_size_ = max_window_size_;
    buckets_.clear();
}

void BitrateStatistics::Update(int64_t bytes, Timestamp at_time) {
    EraseOld(at_time);

    if (!first_update_time_ || num_samples_ == 0) {
        first_update_time_ = at_time;
    }

    if (buckets_.empty() || at_time != buckets_.back().timestamp) {
        if (!buckets_.empty() && at_time < buckets_.back().timestamp) {
            PLOG_WARNING << "Timestamp " << at_time.ms()
                         << " is before the last added timestamp in the rate window: "
                         << buckets_.back().timestamp.ms() << ", aligning to last.";
            at_time = buckets_.back().timestamp;
        }
        buckets_.emplace_back(at_time);
    }
    Bucket& last_bucket = buckets_.back();
    last_bucket.accumulated_bytes += bytes;
    last_bucket.num_samples += 1;
    
    if (accumulated_bytes_ + bytes < std::numeric_limits<int64_t>::max()) {
        accumulated_bytes_ += bytes;
    } else {
        is_overflowed_ = true;
    }
    ++num_samples_;
}

std::optional<DataRate> BitrateStatistics::Rate(Timestamp at_time) const {
    // Erase the obsolete buckets
    // NOTE: Using const_cast is not pretty, but the alternative is to 
    // declare most of the members as mutable.
    const_cast<BitrateStatistics*>(this)->EraseOld(at_time);

    TimeDelta active_window_size = TimeDelta::Zero();
    if (first_update_time_) {
        if (*first_update_time_ + current_window_size_ <= at_time) {
            // If the data stream started before the window, treat
            // window as full even if no data in view currently.
            active_window_size = current_window_size_;
        } else {
            // the window size of a single bucket is 1ms, so even if |first_update_time_ == at_time|
            // the window size should be 1ms.
            active_window_size = at_time - *first_update_time_ + kSingleBucketWindowSize;
        }
    }

    // If window is a single bucket or there is only one sample in a data set
    // that has not grown to the full window size, or if the accumulator has
    // overflowed, treat this as rate unavailable.
    if (num_samples_ == 0 || 
        active_window_size <= kSingleBucketWindowSize ||
        (num_samples_ <= 1 && active_window_size < current_window_size_) ||
        is_overflowed_) {
        return std::nullopt;
    }

    float bitrate_bps = accumulated_bytes_ * 8000.0 / active_window_size.ms() + 0.5f;
    // Better return unavailable rate than garbage value (undefined behavior).
    if (bitrate_bps > static_cast<float>(std::numeric_limits<int64_t>::max())) {
        return std::nullopt;
    }
    return DataRate::BitsPerSec(static_cast<int64_t>(bitrate_bps));
}

bool BitrateStatistics::SetWindowSize(TimeDelta window_size, Timestamp at_time) {
    if (window_size == TimeDelta::Zero() || window_size > max_window_size_) {
        return false;
    }
    if (first_update_time_) {
        // If the window changes (e.g. decreases - removing data point, then
        // increases again) we need to update the first timestamp mark as
        // otherwise it indicates the window coveres a region of zeros, suddenly
        // under-estimating the rate.
        first_update_time_ = std::max(*first_update_time_, at_time - window_size + kSingleBucketWindowSize);
    }
    current_window_size_ = window_size;
    EraseOld(at_time);
    return true;
}

// Private methods
void BitrateStatistics::EraseOld(Timestamp at_time) {
    // New oldest time that is included in data set.
    const Timestamp new_oldest_time = at_time - current_window_size_;

    // Loop over buckets and remove too old data points.
    while (!buckets_.empty() && 
           buckets_.front().timestamp <= new_oldest_time) {
        const Bucket& oldest_bucket = buckets_.front();
        accumulated_bytes_ -= oldest_bucket.accumulated_bytes;
        num_samples_ -= oldest_bucket.num_samples;
        buckets_.pop_front();
        // Reset |is_overflowed_| when having obsolete buckets be removed
        if (is_overflowed_ && accumulated_bytes_ < std::numeric_limits<int64_t>::max()) {
            is_overflowed_ = false;
        }
    }
    
}
    
} // namespace naivertc
