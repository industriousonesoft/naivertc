#include "rtc/congestion_control/components/interval_budget.hpp"

#include <algorithm>

namespace naivertc {
namespace {
    
constexpr TimeDelta kBudgetWindow = TimeDelta::Millis(500); // 500ms

} // namespace


IntervalBudget::IntervalBudget(DataRate initial_target_bitrate, 
                               bool can_build_up_underuse) 
    : target_bitrate_(DataRate::Zero()),
      bytes_remaining_(0),
      can_build_up_from_underuse_(can_build_up_underuse) {
    set_target_bitrate(initial_target_bitrate);
}
    
IntervalBudget::~IntervalBudget() {}

DataRate IntervalBudget::target_bitrate() const {
    return target_bitrate_;
}

size_t IntervalBudget::bytes_remaining() const {
    return static_cast<size_t>(std::max<int64_t>(0, bytes_remaining_));
}

double IntervalBudget::budget_ratio() const {
    if (max_bytes_in_budget_ <= 0) {
        return 0.0;
    }
    return static_cast<double>(bytes_remaining_) / max_bytes_in_budget_;
}

void IntervalBudget::set_target_bitrate(DataRate bitrate) {
    target_bitrate_ = bitrate;
    max_bytes_in_budget_ = static_cast<int64_t>(bitrate * kBudgetWindow);
    // Clamps |bytes_remaining_| in [-max_bytes_in_budget_, max_bytes_in_budget_].
    bytes_remaining_ = std::min(std::max(-max_bytes_in_budget_, bytes_remaining_), max_bytes_in_budget_);
}

void IntervalBudget::IncreaseBudget(TimeDelta interval_time) {
    int64_t increased_bytes = static_cast<int64_t>(target_bitrate_ * interval_time);
    const bool overused_last_interval = bytes_remaining_ < 0;
    if (overused_last_interval || can_build_up_from_underuse_) {
        // We overuse last interval, compensate this interval.
        // Make sure the compensated result is not more than the |max_bytes_in_budget_|.
        bytes_remaining_ = std::min(bytes_remaining_ + increased_bytes, max_bytes_in_budget_);
    } else {
        // If we underused last interval, not use the last |bytes_remaining_| this interval.
        bytes_remaining_ = std::min(increased_bytes, max_bytes_in_budget_);
    }
}

void IntervalBudget::ConsumeBudget(size_t bytes) {
    bytes_remaining_ = std::max(bytes_remaining_ - static_cast<int>(bytes), -max_bytes_in_budget_);
}
    
} // namespace naivertc
