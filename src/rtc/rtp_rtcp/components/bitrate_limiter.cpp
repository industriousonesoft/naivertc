#include "rtc/rtp_rtcp/components/bitrate_limiter.hpp"
#include "rtc/base/time/clock.hpp"

namespace naivertc {

BitrateLimiter::BitrateLimiter(Clock* clock, TimeDelta max_window_size) 
    : clock_(clock),
      bitrate_stats_(max_window_size),
      curr_window_size_(max_window_size),
      max_bitrate_(DataRate::PlusInfinity()) {}

BitrateLimiter::~BitrateLimiter() = default;

void BitrateLimiter::set_max_bitrate(DataRate max_bitrate) {
    max_bitrate_ = max_bitrate;
}

bool BitrateLimiter::SetWindowSize(TimeDelta window_size) {
    curr_window_size_ = window_size;
    return bitrate_stats_.SetWindowSize(window_size, clock_->CurrentTime());
}

bool BitrateLimiter::TryConsumeBitrate(size_t bytes) {
    auto now = clock_->CurrentTime();
    auto curr_bitrate = bitrate_stats_.Rate(now);
    if (curr_bitrate) {
        // If there is a available bitrate, check if adding bytes would
        // cause maximum bitrate target to be exceeded. 
        DataRate bitrate_addition = bytes / curr_window_size_;
        if (*curr_bitrate + bitrate_addition > max_bitrate_) {
            return false;
        }
    } else {
        // If there is no available bitrate, allow allocating bitrate
        // even if target is exceeded. This prevents problems at very
        // low bitrates, where for instance retransmissions would never
        // be allowed due to too high bitrate caused by a sinble packet.
    }
    bitrate_stats_.Update(bytes, now);
    return true;
}
    
} // namespace naivertc
