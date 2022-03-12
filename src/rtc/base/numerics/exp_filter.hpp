#ifndef _RTC_BASE_NUMERICS_EXP_FILTER_H_
#define _RTC_BASE_NUMERICS_EXP_FILTER_H_

#include <optional>

namespace naivertc {

// This class is a smoothing filter, can be used to smooth 
// bandwidth estimation or packet loss estimation.
class ExpFilter {
public:
    explicit ExpFilter(float alpha, 
                       std::optional<float> filtered_value_cap = std::nullopt);

    float alpha() const;
    void set_alpha(float alpha);

    // Returns current filtered value.
    std::optional<float> filtered() const;

    // Resets the filter to its intial state.
    void Reset(float alpha);

    // Applies the filter with a given exponent on the provided sample:
    // y(k) = min(alpha_^ exp * y(k-1) + (1 - alpha_^ exp) * sample, max_).
    float Apply(float exp, float sample);

private:
    const std::optional<float> filtered_value_cap_;
    float alpha_; 
    std::optional<float> filtered_value_;
};
    
} // namespace naivertc

#endif