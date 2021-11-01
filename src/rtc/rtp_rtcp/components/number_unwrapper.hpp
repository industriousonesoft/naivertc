#ifndef _RTC_RTP_RTCP_COMPONENTS_NUMBER_UNWRAPPER_H_
#define _RTC_RTP_RTCP_COMPONENTS_NUMBER_UNWRAPPER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_checker.hpp"

#include <limits>
#include <optional>

namespace naivertc {
// TODO: Replace with `wrap_around_utils`
template <typename U>
class RTC_CPP_EXPORT NumberUnwrapper {
    static_assert(!std::numeric_limits<U>::is_signed, "U must be unsigned.");
    static_assert(std::numeric_limits<U>::max() <= std::numeric_limits<uint32_t>::max(), "U must not be wider than 32 bits");
public:
    int64_t Unwrap(U value, bool update_last = true);

    std::optional<int64_t> last_value() const { return last_value_; }
    void UpdateLast(int64_t last_value) { last_value_ = last_value; }

private:
    std::optional<int64_t> last_value_;
};

template <typename U>
int64_t NumberUnwrapper<U>::Unwrap(U value, bool update_last) {
    int64_t unwrapped = 0;
    if (!last_value_.has_value()) {
        unwrapped = value;
    }else {
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
        unwrapped = *last_value_ + delta;
    }
    if (update_last) {
        last_value_ = unwrapped;
    }
    return unwrapped;
}

using SequenceNumberUnwrapper = NumberUnwrapper<uint16_t>;
using TimestampUnwrapper = NumberUnwrapper<uint32_t>;

} // namespace naivertc

#endif