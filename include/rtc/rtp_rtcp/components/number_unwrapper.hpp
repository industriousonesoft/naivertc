#ifndef _RTC_RTP_RTCP_COMPONENTS_NUMBER_UNWRAPPER_H_
#define _RTC_RTP_RTCP_COMPONENTS_NUMBER_UNWRAPPER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/components/number_wrap_checker.hpp"

#include <limits>
#include <optional>

namespace naivertc {

template <typename U>
class RTC_CPP_EXPORT NumberUnwrapper {
    static_assert(!std::numeric_limits<U>::is_signed, "U must be unsigned.");
    static_assert(std::numeric_limits<U>::max() <= std::numeric_limites<uint32_t>::max(), "U must not be wider than 32 bits");
public:
    NumberUnwrapper();
    ~NumberUnwrapper();

    int64_t Unwrap(U value) const;

    std::optional<int64_t> last_value const { return last_value_; }
    void UpdateLast(int64_t last_value) { last_value_ = last_value; }

private:
    std::optional<int64_t> last_value_;
};

template <typename U>
int64_t Unwrap(U value) const {
    if (!last_value_.has_value()) {
        return value;
    }
    constexpr int64_t kMaxPlusOne = static_cast<int64_t>(std::numeric_limits<U>::max()) + 1;
    U cropped_last = static_cast<U>(*last_value_);
    int64_t delta = value - cropped_last;
    if (IsNewer(value, cropped_last)) {
        // Forward wrap-around
        if (delta < 0) {
            delta += kMaxPlusOne;
        }
    }else if (delta > 0 && (*last_value_ + delta - kMaxPlusOne) >= 0) {
        // If value is older but delta is positive, this is a backwards wrap-around.
        // However, don't wrap backwards past 0.
        delta -= kMaxPlusOne;
    }
    return *last_value_ + delta;
}

} // namespace naivertc

#endif