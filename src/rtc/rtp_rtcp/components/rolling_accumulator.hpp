#ifndef _RTC_RTP_RTCP_COMPONENTS_ROLLING_ACCUMULATOR_H_
#define _RTC_RTP_RTCP_COMPONENTS_ROLLING_ACCUMULATOR_H_

#include "base/defines.hpp"
#include "rtc/base/numerics/running_statistics.hpp"

#include <vector>

namespace naivertc {

// This class is used to store and report statistics
// over N most recent samples.
template<typename T,
        typename std::enable_if<std::is_convertible<T, double>::value, T>::type* = nullptr>
class RTC_CPP_EXPORT RollingAccumulator {
public:
    explicit RollingAccumulator(size_t max_count) 
        : samples_(max_count) {
        assert(max_count > 0);
        Reset();
    }
    ~RollingAccumulator() = default;

    void Reset() {
       stats_.Reset();
       next_index_ = 0;
       max_ = T();
       max_stale_ = false;
       min_ = T();
       min_stale_ = false;
    }

    size_t max_count() const { return samples_.size(); }
    size_t count() const { return static_cast<size_t>(stats_.sample_count()); }

    void AddSample(T sample) {
        // Remove oldest sample.
        if (count() == max_count()) {
            // NOTE: When the `samples_` is overflow, 
            // the `next_index_` points to the oldest sample, 
            // since the `samples_` is used as a circular array.
            T sample_to_remove = samples_[next_index_];
            stats_.RemoveSample(sample_to_remove);
            if (sample_to_remove >= max_) {
                max_stale_ = true;
            }
            if (sample_to_remove <= min_) {
                min_stale_ = true;
            }
        }
        // Add new sample.
        samples_[next_index_] = sample;
        if (count() == 0 || sample > max_) {
            max_ = sample;
            max_stale_ = false;
        }
        if (count() == 0 || sample < min_) {
            min_ = sample;
            min_stale_ = false;
        }
        stats_.AddSample(sample);
        // Update next index.
        next_index_ = (next_index_ + 1) % max_count();
    }

    T ComputeMax() const {
        // Find the new max value if the old one is stale.
        if (max_stale_) {
            max_ = samples_[next_index_];
            for (size_t i = 1; i < count(); ++i) {
                max_ = std::max(max_, samples_[(next_index_ + i) % max_count()]);
            }
            max_stale_ = false;
        }
        return max_;
    }

    T ComputeMin() const {
        // Find the new max value if the old one is stale.
        if (min_stale_) {
            min_ = samples_[next_index_];
            for (size_t i = 1; i < count(); ++i) {
                min_ = std::min(min_, samples_[(next_index_ + i) % max_count()]);
            }
            min_stale_ = false;
        }
        return min_;
    }

    // All samples with the same weight.
    double ComputeMean() const { return stats_.mean().value_or(0.0); }

    // Weights nth sample with weight (leaning_rate)^n.
    double ComputeWeightedMean(double learning_rate) const {
        // `learning_rate` should be between (0.0, 1.0], 
        // otherwise the non-weighted mean is retured.
        if (count() < 1 || learning_rate <= 0.0 || learning_rate >= 1.0) {
            return ComputeMean();
        }

        double curr_weight = 1.0;
        double weighted_sample_sum = 0.0;
        double weight_sum = 0.0;
        size_t curr_index = next_index_ - 1;
        const size_t max_size = max_count();
        for (size_t i = 0; i < count(); ++i) {
            curr_weight *= learning_rate;
            weight_sum += curr_weight;
            // The newer sample owns a bigger weight.
            // Add `max_size` to prevent underflow.
            size_t index = (curr_index + max_size - i) % max_size;
            weighted_sample_sum += curr_weight * samples_[index];
        }
        return weighted_sample_sum / weight_sum;
    }

    // Compute estimated variance, and the result is more accurate as 
    // the number of samples grows.
    double ComputeVariance() const { return stats_.Variance().value_or(0.0); }

private:
    RunningStatistics<T> stats_;
    std::vector<T> samples_;
    size_t next_index_;
    mutable T max_;
    mutable bool max_stale_;
    mutable T min_;
    mutable bool min_stale_;

    DISALLOW_COPY_AND_ASSIGN(RollingAccumulator);
};
    
} // namespace naivertc


#endif