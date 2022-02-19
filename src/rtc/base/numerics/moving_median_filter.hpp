#ifndef _RTC_BASE_NUMERICS_MOVING_MEDIAN_FILTER_H_
#define _RTC_BASE_NUMERICS_MOVING_MEDIAN_FILTER_H_

#include "base/defines.hpp"
#include "rtc/base/numerics/percentile_filter.hpp"

#include <list>

namespace naivertc {

// Class to efficiently get moving median filter from a stream of samples
template <typename T>
class MovingMedianFilter {
public:
    // `window_size` is how many lastest samples are stored and
    // used to take median. it must be positive.
    explicit MovingMedianFilter(size_t window_size);

    void Insert(const T& value);

    void Reset();

    T GetFilteredValue() const;

    // The count of stored samples currently
    size_t stored_sample_count() const {  return stored_sample_count_; };

private:
    const size_t window_size_;
    PercentileFilter<T> percentile_filter_;
    std::list<T> samples_;
    size_t stored_sample_count_;

    DISALLOW_COPY_AND_ASSIGN(MovingMedianFilter);
};

template <typename T>
MovingMedianFilter<T>::MovingMedianFilter(size_t window_size) 
    : window_size_(window_size),
      percentile_filter_(0.5f),
      stored_sample_count_(0) {
    assert(window_size > 0);
}

template <typename T>
void MovingMedianFilter<T>::Insert(const T& value) {
    percentile_filter_.Insert(value);
    samples_.emplace_back(value);
    ++stored_sample_count_;
    if (stored_sample_count_ > window_size_) {
        percentile_filter_.Erase(samples_.front());
        samples_.pop_front();
        --stored_sample_count_;
    }
}

template <typename T>
T MovingMedianFilter<T>::GetFilteredValue() const {
    return percentile_filter_.GetPercentileValue();
}

template <typename T>
void MovingMedianFilter<T>::Reset() {
    percentile_filter_.Reset();
    samples_.clear();
    stored_sample_count_ = 0;
}
    
} // namespace naivertc


#endif