#ifndef _RTC_RTP_RTCP_COMPONENTS_SEQ_NUM_UNWRAPPER_H_
#define _RTC_RTP_RTCP_COMPONENTS_SEQ_NUM_UNWRAPPER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

#include <optional>

namespace naivertc {

template <typename T, T M = 0>
class NumberUnwrapper {
static_assert(
      std::is_unsigned<T>::value &&
      std::numeric_limits<T>::max() < std::numeric_limits<int64_t>::max(),
      "Type unwrapped must be an unsigned integer smaller than int64_t.");
public:
    int64_t Unwrap(T value, bool disallow_negative = true) {
        if (!last_value_) {
            last_unwrapped_ = {value};
        } else {
            T last_value = *last_value_;
            // Forward
            if (wrap_around_utils::AheadOrAt<T, M>(value, last_value)) {
                last_unwrapped_ += ForwardDiff<T, M>(last_value, value);
            // Backward
            } else {
                last_unwrapped_ -= ReverseDiff<T, M>(last_value, value);
                // Don't wrap backwards past 0.
                if (disallow_negative && last_unwrapped_ < 0) {
                    // `kBackwardAdjustment` is the max number count the type T can represent.
                    // For instance, for a uint16_t it will be 2^32 (uint32_max + 1).
                    constexpr int64_t kBackwardAdjustment = M == 0 ? int64_t{std::numeric_limits<T>::max()} + 1 : M;
                    last_unwrapped_ += kBackwardAdjustment;
                }
            }
        }

        last_value_ = value;
        return last_unwrapped_;
    }

    int64_t UnwrapForward(T value) {
        if (!last_value_) {
            last_unwrapped_ = {value};
        } else {
            last_unwrapped_ += ForwardDiff<T, M>(*last_value_, value);
        }

        last_value_ = value;
        return last_unwrapped_;
    }

    int64_t UnwrapBackwards(T value) {
        if (!last_value_) {
            last_unwrapped_ = {value};
        } else {
            last_unwrapped_ -= ReverseDiff<T, M>(*last_value_, value);
        }

        last_value_ = value;
        return last_unwrapped_;
    }

private:
    T last_wrapped_value() const {
        // FIXME: Not working if allowing to wrap backwards past 0, using `last_value_` instead.
        assert(last_unwrapped_ >= 0);
        // if M == 0, M = 1 + the max value of type T.
        if (M == 0) {
            // The higher bytes exceed the max value of type T is the count of wrap around,
            // and the rest lower bytes of type T is the last value. 
            return static_cast<T>(last_unwrapped_);
        } else {
            T num_wrap_around = (last_unwrapped_ / M);
            return static_cast<T>(last_unwrapped_ - num_wrap_around * M);
        }
    }

private:
    int64_t last_unwrapped_ = 0;
    std::optional<T> last_value_;
};

} // namespace naivertc


#endif