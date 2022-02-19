#ifndef _RTC_RTP_RTCP_COMPONENTS_NUMBER_UNWRAPPER_H_
#define _RTC_RTP_RTCP_COMPONENTS_NUMBER_UNWRAPPER_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/components/wrap_around_utils.hpp"

#include <limits>
#include <optional>

namespace naivertc {
    
// This class is a specific of `NumberUnwrapper` with the max value of type U as the modulo.
template <typename U>
class NumberUnwrapper {
    static_assert(!std::numeric_limits<U>::is_signed, "U must be unsigned.");
    static_assert(std::numeric_limits<U>::max() <= std::numeric_limits<uint32_t>::max(), "U must not be wider than 32 bits");
public:
    int64_t Unwrap(U value, bool update_last = true);

    std::optional<int64_t> Last() const { return last_unwrapped_value_; }
    void UpdateLast(int64_t value) { last_unwrapped_value_ = value; }

private:
    std::optional<int64_t> last_unwrapped_value_;
};

template <typename U>
int64_t NumberUnwrapper<U>::Unwrap(U value, bool update_last) {
    int64_t unwrapped = 0;
    if (!last_unwrapped_value_) {
        unwrapped = value;
    } else {
        // The higher bytes exceed the max value of type U is the count of wrap around,
        // and the rest lower bytes of type U is the last value.
        // Last wrapped value 
        U last_value = static_cast<U>(*last_unwrapped_value_);
        
        // Forward
        if (wrap_around_utils::AheadOrAt<U>(value, last_value)) {
            unwrapped = *last_unwrapped_value_ + ForwardDiff<U>(last_value, value);
        // Backward
        } else {
            unwrapped = *last_unwrapped_value_ - ReverseDiff<U>(last_value, value);
            // Don't wrap backwards past 0.
            if (unwrapped < 0) {
                // `kBackwardAdjustment` is the max number count the type T can represent.
                // For instance, for a uint16_t it will be 2^32 (uint32_max + 1).
                constexpr int64_t kBackwardAdjustment = static_cast<int64_t>(std::numeric_limits<U>::max()) + 1;
                unwrapped += kBackwardAdjustment;
            }
        }
    }
    if (update_last) {
        last_unwrapped_value_ = unwrapped;
    }
    return unwrapped;
}

using SeqNumUnwrapper = NumberUnwrapper<uint16_t>;
using TimestampUnwrapper = NumberUnwrapper<uint32_t>;

} // namespace naivertc

#endif