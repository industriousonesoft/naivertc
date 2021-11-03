#ifndef _RTC_BASE_NUMERICS_RUNNING_STATISTICS_H_
#define _RTC_BASE_NUMERICS_RUNNING_STATISTICS_H_

#include "base/defines.hpp"

#include <type_traits>
#include <limits>
#include <optional>

namespace naivertc {
namespace {

template<typename T>
T InfinityOrMaxValue() {
    return std::numeric_limits<T>::has_infinity ? std::numeric_limits<T>::infinity() 
                                                : std::numeric_limits<T>::max();
}

template<typename T>
T MinusInfinityOrMinValue() {
    return std::numeric_limits<T>::has_infinity ? -std::numeric_limits<T>::infinity() 
                                                : std::numeric_limits<T>::min();
}

} // namespac 

// using Welford's method for variance.
// See https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_online_algorithm
template<typename T,
         typename std::enable_if<std::is_convertible<T, double>::value, T>::type* = nullptr>
class RTC_CPP_EXPORT RunningStatistics {
public:
    RunningStatistics() 
        : count_(0),
          min_(InfinityOrMaxValue<T>()),
          max_(MinusInfinityOrMinValue<T>()),
          mean_(0.0),
          cumulated_variance_(0.0) {}

    void AddSample(T sample) {
        max_ = std::max(max_, sample);
        min_ = std::min(min_, sample);
        ++count_;
        const double delta = sample - mean_;
        mean_ += delta / count_;
        const double delta2 = sample - mean_;
        cumulated_variance_ += delta * delta2;
    }

    void RemoveSample(T sample) {
        if (count_ <= 0) {
            return;
        }
        --count_;
        const double delta = sample - mean_;
        mean_ -= delta / count_;
        const double delta2 = sample - mean_;
        cumulated_variance_ -= delta * delta2;
    }

    // Merge other stats, as if samples were added one by one.
    void Merge(const RunningStatistics<T>& other) {
        if (other.count_ <= 0) {
            return;
        }
        max_ = std::max(max_, other.max_);
        min_ = std::min(min_, other.min_);
        const int64_t merged_count = count_ + other.count_;
        const double merged_mean = (mean_ * count_ + other.mean_ * other.count_) / merged_count;
        // Calculate new `cumulated_variance_`: from sum((x_i - mean_)²) to sum((x_i - new_mean)²).
        auto cumulated_variance_delta = [merged_mean](const RunningStatistics& stats) {
            const double mean_delta = merged_mean - stats.mean_;
            return stats.count_ * (mean_delta * mean_delta);
        };
        cumulated_variance_ = cumulated_variance_ + cumulated_variance_delta(*this) +
                              other.cumulated_variance_ + cumulated_variance_delta(other);
        mean_ = merged_mean;
        count_ = merged_count;
    }

    int64_t sample_count() const { return count_; }

    std::optional<T> min() const { return count_ > 0 ? std::optional<T>(min_) : std::nullopt; }
    std::optional<T> max() const { return count_ > 0 ? std::optional<T>(max_) : std::nullopt; }
    std::optional<double> mean() const { return count_ > 0 ? std::optional<double>(mean_) : std::nullopt; }
    std::optional<double> Variance() const { return count_ > 0 ? std::optional<double>(cumulated_variance_ / count_) : std::nullopt; }
    std::optional<double> StandardDeviation() const { return count_ > 0 ? std::optional<double>(sqrt(*Variance())) : std::nullopt; }

private:
    int64_t count_;
    T min_;
    T max_;
    double mean_;
    double cumulated_variance_;
};
    
} // namespace naivertc


#endif