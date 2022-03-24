#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/rtt_filter.hpp"

namespace naivertc {
namespace rtp {
namespace video {
namespace jitter {
namespace {

constexpr int64_t kMaxRttMs = 3000; // 3s

constexpr uint8_t kMaxSampleCount = 35;
constexpr double kJumpStandardDeviation = 2.5;
constexpr double kDriftStandardDeviation = 3.5;
    
} // namespace 


RttFilter::RttFilter() 
    : has_first_non_zero_update_(false),
      avg_rtt_(0),
      var_rtt_(0),
      max_rtt_(0),
      jump_count_(0),
      drift_count_(0),
      sample_count_(1) {}

RttFilter& RttFilter::operator=(const RttFilter& rhs) {
    if (this != &rhs) {
        has_first_non_zero_update_ = rhs.has_first_non_zero_update_;
        avg_rtt_ = rhs.avg_rtt_;
        var_rtt_ = rhs.var_rtt_;
        max_rtt_ = rhs.max_rtt_;
        jump_count_ = rhs.jump_count_;
        drift_count_ = rhs.drift_count_;
        sample_count_ = rhs.sample_count_;
        memcpy(jump_buffer_, rhs.jump_buffer_, sizeof(rhs.jump_buffer_));
        memcpy(drift_buffer_, rhs.drift_buffer_, sizeof(rhs.drift_buffer_));
    }
    return *this;
}

void RttFilter::AddRtt(int64_t rtt_ms) {
    // Check if we have got the first non-zero rtt value.
    if (!has_first_non_zero_update_) {
        if (rtt_ms == 0) {
            return;
        }
        has_first_non_zero_update_ = true;
    }

    // Sanity check
    if (rtt_ms > kMaxRttMs) {
        rtt_ms = kMaxRttMs;
    }

    double filt_factor = 0;
    if (sample_count_ > 1) {
        filt_factor = static_cast<double>(sample_count_ - 1) / sample_count_;
    }
    ++sample_count_;
    // Prevent `filt_factor` from going above.
    if (sample_count_ > kMaxSampleCount) {
        // (kMaxSampleCount - 1) / kMaxSampleCount
        // e.g., kMaxSampleCount = 50 => filt_factor = (50 - 1) / 50 = 0.98;
        sample_count_ = kMaxSampleCount;
    }
    double old_avg_rtt = avg_rtt_;
    double old_var_rtt = var_rtt_;
    // NOTE: The moving average algorithm returns a more smooth and robust result.
    // Calculate average rtt (using moving average algorithm)
    avg_rtt_ = filt_factor * avg_rtt_ + (1 - filt_factor) * rtt_ms;
    // Calculate variance rtt (using moving average algorithm)
    var_rtt_ = filt_factor * var_rtt_ + (1 - filt_factor) * pow(rtt_ms - avg_rtt_, 2);
    max_rtt_ = std::max(max_rtt_, rtt_ms);
    // In some cases we don't want to update the statistics
    if (!JumpDetection(rtt_ms, avg_rtt_, var_rtt_) || !DriftDetection(rtt_ms, avg_rtt_, var_rtt_, max_rtt_)) {
        avg_rtt_ = old_avg_rtt;
        var_rtt_ = old_var_rtt;
    }
}

void RttFilter::Reset() {
    has_first_non_zero_update_ = false;
    avg_rtt_ = 0;
    var_rtt_ = 0;
    max_rtt_ = 0;
    jump_count_ = 0;
    drift_count_ = 0;
    sample_count_ = 1;
    // NOTE: memset是按字节赋值的，切勿用于非单字节（char, uint8_t)类型数组的赋值或多字节数组赋值非0值.
    // `memset` will set value byte by byte, the value to set will be truncated as uint8_t.
    // e.g., int a[2]; memset(a, 0x1203, sizeof(a)) => 
    // expected: 0x00000003, 0x00000003
    // actual: 0x03030303, 0x03030303
    memset(jump_buffer_, 0, sizeof(jump_buffer_));
    memset(drift_buffer_, 0, sizeof(drift_buffer_));
}

int64_t RttFilter::RttMs() const {
    // FIXME: Why we need to round a integer?
    return static_cast<int64_t>(max_rtt_ + 0.5);
}

// Private methods
bool RttFilter::JumpDetection(int64_t rtt_ms, int64_t avg_rtt, int64_t var_rtt) {
    double diff_from_avg = avg_rtt - rtt_ms;
    // There has a big diff between `rtt_ms` and `avg_rtt`, which means
    // a jump may happens.
    if (fabs(diff_from_avg) > kJumpStandardDeviation * sqrt(var_rtt)) {
        int diff_sign = (diff_from_avg >= 0) ? 1 : -1;
        int jump_count_sign = (jump_count_ >= 0) ? 1 : -1;
        
        if (diff_sign != jump_count_sign) {
            // Since the signs differ the samples currently in the 
            // buffer is useless as they represent a jump in a 
            // different direction.
            jump_count_ = 0;
        }
        // Accumulate the jump count in a same direction.
        if (abs(jump_count_) < kDetectThreshold) {
            // Update the buffer used for the short time statistics.
            // The sign of the diff is used for updating the counter
            // since we want to use the same buffer for keeping track
            // of when the RTT jumps down and up.
            jump_buffer_[abs(jump_count_)] = rtt_ms;
            jump_count_ += diff_sign;
        }
        // Detected an RTT jump.
        if (abs(jump_count_) >= kDetectThreshold) {
            UpdateRtts(jump_buffer_, abs(jump_count_));
            // Short rtt filter.
            sample_count_ = kDetectThreshold + 1;
            jump_count_ = 0;
        } else {
            return false;
        }
    } else {
        jump_count_ = 0;
    }
    return true;
}

bool RttFilter::DriftDetection(int64_t rtt_ms, int64_t avg_rtt, int64_t var_rtt, int64_t max_rtt) {
    // There has a big diff between `max_rtt` and `avg_rtt`, which means
    // a drift may happens.
    if (max_rtt - avg_rtt > kDriftStandardDeviation * sqrt(var_rtt)) {
        // Accumulate the drift count.
        if (drift_count_ < kDetectThreshold) {
            // Update the buffer used for the short time statistics.
            drift_buffer_[drift_count_] = rtt_ms;
            ++drift_count_;
        }
        // Detected an RTT drift.
        if (drift_count_ >= kDetectThreshold) {
            UpdateRtts(drift_buffer_, drift_count_);
            // Short rtt filter.
            sample_count_ = kDetectThreshold + 1;
            drift_count_ = 0;
        }
    } else {
        drift_count_ = 0;
    }
    return true;
}

void RttFilter::UpdateRtts(int64_t* buf, uint8_t count) {
    if (count == 0) {
        return;
    }
    max_rtt_ = 0;
    avg_rtt_ = 0;
    for (uint8_t i = 0; i < count; ++i) {
        // Find the max rtt.
        if (buf[i] > max_rtt_) {
            max_rtt_ = buf[i];
        }
        avg_rtt_ += buf[i];
    }
    avg_rtt_ = avg_rtt_ / static_cast<double>(count);
}
    
} // namespace jitter
} // namespace video
} // namespace rtp
} // namespace naivertc