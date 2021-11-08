#ifndef _RTC_RTP_RTCP_COMPONENTS_SEQ_NUM_UNWRAPPER_H_
#define _RTC_RTP_RTCP_COMPONENTS_SEQ_NUM_UNWRAPPER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

#include <optional>

namespace naivertc {

template <typename T, T M = 0>
class RTC_CPP_EXPORT SeqNumUnwrapper {
static_assert(
      std::is_unsigned<T>::value &&
          std::numeric_limits<T>::max() < std::numeric_limits<int64_t>::max(),
      "Type unwrapped must be an unsigned integer smaller than int64_t.");
public:
    int64_t last_unwrapped_value() const { return last_unwrapped_; }
    void set_last_unwrapped_value(int64_t value) { last_unwrapped_ = value; }

    int64_t Unwrap(T value, bool disallow_negative = true) {
        if (!last_value_) {
            last_unwrapped_ = {value};
        } else {
            T last_value = *last_value_;
            // Forward
            if (wrap_around_utils::AheadOrAt<T, M>(value, last_value)) {
                // constexpr int64_t kBackwardAdjustment = M == 0 ? int64_t{std::numeric_limits<T>::max()} + 1 : M;
                // last_unwrapped_ -= kBackwardAdjustment;
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
        // if M == 0, M = 1 + the max value of type T.
        // The bytes exceed the max value of type T is the count of wrap around,
        // and the bytes of type T is the last value.
        // FIXME: Why this not always works, i.e; it's falied to past the unit test `ManyBackwardWraps`? 
        return M == 0 ? static_cast<T>(last_unwrapped_) : static_cast<T>(last_unwrapped_) % M;
    }

private:
    int64_t last_unwrapped_ = 0;
    std::optional<T> last_value_;
};
    
} // namespace naivertc


#endif