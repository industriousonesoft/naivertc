#ifndef _RTC_BASE_NUMERICS_HISTOGRAM_PERCENTILE_COUNTER_H_
#define _RTC_BASE_NUMERICS_HISTOGRAM_PERCENTILE_COUNTER_H_

#include "base/defines.hpp"

#include <vector>
#include <optional>
#include <map>

namespace naivertc {

class RTC_CPP_EXPORT HistogramPercentileCounter {
public:
    // Values below `long_tail_boundary` are stored as the histogram in an array.
    // Values above - in a map.
    explicit HistogramPercentileCounter(uint32_t long_tail_boundary);
    ~HistogramPercentileCounter();
    void Add(uint32_t value);
    void Add(uint32_t value, size_t count);
    void Add(const HistogramPercentileCounter& other);
    // Argument should be from 0 to 1.
    std::optional<uint32_t> GetPercentile(float fraction);

 private:
    std::vector<size_t> histogram_low_;
    std::map<uint32_t, size_t> histogram_high_;
    const uint32_t long_tail_boundary_;
    size_t total_elements_;
    size_t total_elements_low_;
};
    
} // namespace naivertc


#endif