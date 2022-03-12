#include "rtc/base/numerics/exp_filter.hpp"

#include <cmath>

namespace naivertc {

ExpFilter::ExpFilter(float alpha, 
                     std::optional<float> filtered_value_cap) 
    : filtered_value_cap_(filtered_value_cap),
      alpha_(alpha),
      filtered_value_(std::nullopt) {

}

float ExpFilter::alpha() const {
    return alpha_;
}

void ExpFilter::set_alpha(float alpha) {
    alpha_ = alpha;
}

std::optional<float> ExpFilter::filtered() const {
    return filtered_value_;
}

void ExpFilter::Reset(float alpha) {
    alpha_ = alpha;
    filtered_value_.reset();
}

float ExpFilter::Apply(float exp, float sample) {
    if (!filtered_value_) {
        // Initialize filtered value.
        filtered_value_ = sample;
    } else if (exp == 1.0) {
        filtered_value_ = alpha_ * filtered_value_.value() + (1 - alpha_) * sample;
    } else {
        float alpha = std::pow(alpha_, exp);
        filtered_value_ = alpha * filtered_value_.value() + (1 - alpha) * sample;
    }
    if (filtered_value_cap_ && filtered_value_ > filtered_value_cap_) {
        filtered_value_ = filtered_value_cap_;
    }
    return filtered_value_.value();
}
    
} // namespace naivertc
