#ifndef _RTC_BASE_NUMERICS_PERCENTILE_FILTER_H_
#define _RTC_BASE_NUMERICS_PERCENTILE_FILTER_H_

#include "base/defines.hpp"

#include <iterator>
#include <set>

namespace naivertc {

// Class to efficiently get the percentile value from a 
// group of observations. The percentile is the value below
// which a given percentage of the observations fall.
template <typename T>
class PercentileFilter {
public:
    // `percentile` should be between 0 and 1
    explicit PercentileFilter(float percentile);
    
    // Insert one observation.
    void Insert(const T& value);

    // Remove one observation or return false if `value`
    // dosen't exist in the container.
    bool Erase(const T& value);

    // Get the percentile value.
    T GetPercentileValue() const;

    void Reset();

private:
    void Refresh();

private:
    const float percentile_;
    std::multiset<T> set_;
    int64_t percentile_index_;
    // Maintain iterator and index of current target percentile value.
    typename std::multiset<T>::iterator percentile_it_;
};

// Implements
template <typename T>
PercentileFilter<T>::PercentileFilter(float percentile) 
    : percentile_(percentile),
      percentile_it_(set_.begin()),
      percentile_index_(0) {
    assert(percentile >= 0.0);
    assert(percentile <= 1.0);
}

template <typename T>
void PercentileFilter<T>::Insert(const T& value) {
    set_.insert(value);
    if (set_.size() == 1u) {
        percentile_it_ = set_.begin();
        percentile_index_ = 0;
    }else if (value < *percentile_it_) {
        ++percentile_index_;
    }
    Refresh();
}

template <typename T>
bool PercentileFilter<T>::Erase(const T& value) {
    typename std::multiset<T>::const_iterator it = set_.lower_bound(value);
    // Ignore if the element is not present in the current set.
    if (it == set_.end() || *it != value) {
        return false;
    }
    // If same iterator, update to the following element. Index is not affected.
    if (it == percentile_it_) {
        percentile_it_ = set_.erase(it);
    }else {
        set_.erase(it);
        // If erased element was before us, decrement `percentile_index_`
        if (value <= *percentile_it_) {
            --percentile_index_;
        }
    }
    Refresh();
    return true;
}

template <typename T>
T PercentileFilter<T>::GetPercentileValue() const {
    return set_.empty() ? 0 : *percentile_it_;
}

template <typename T>
void PercentileFilter<T>::Reset() {
    set_.clear();
    percentile_it_ = set_.begin();
    percentile_index_ = 0;
}

template <typename T>
void PercentileFilter<T>::Refresh() {
    if (set_.empty()) {
        return;
    }
    const int64_t index = static_cast<int64_t>(percentile_ * (set_.size() - 1));
    std::advance(percentile_it_, index - percentile_index_);
    percentile_index_ = index;
}
    
} // namespace naivertc


#endif