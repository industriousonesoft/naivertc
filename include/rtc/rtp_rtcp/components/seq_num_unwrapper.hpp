#ifndef _RTC_RTP_RTCP_COMPONENTS_SEQ_NUM_UNWRAPPER_H_
#define _RTC_RTP_RTCP_COMPONENTS_SEQ_NUM_UNWRAPPER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/components/seq_num_utils.hpp"

#include <optional>

namespace naivertc {

template <typename T, T M = 0>
class RTC_CPP_EXPORT SeqNumUnwrapper {
static_assert(
      std::is_unsigned<T>::value &&
          std::numeric_limits<T>::max() < std::numeric_limits<int64_t>::max(),
      "Type unwrapped must be an unsigned integer smaller than int64_t.");
public:
    int64_t Unwrap(T value) {
        if (!last_value_) {
            last_unwrapped_ = {value};
        } else {
            last_unwrapped_ += ForwardDiff<T, M>(*last_value_, value);

            if (!seq_num_utils::AheadOrAt<T, M>(value, *last_value_)) {
                constexpr int64_t kBackwardAdjustment =  M == 0 ? int64_t{std::numeric_limits<T>::max()} + 1 : M;
                last_unwrapped_ -= kBackwardAdjustment;
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
    int64_t last_unwrapped_ = 0;
    std::optional<T> last_value_;
};
    
} // namespace naivertc


#endif