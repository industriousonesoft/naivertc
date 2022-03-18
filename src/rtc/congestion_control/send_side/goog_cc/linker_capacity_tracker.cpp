#include "rtc/congestion_control/send_side/goog_cc/linker_capacity_tracker.hpp"

#include <plog/Log.h>

namespace naivertc {

LinkerCapacityTracker::LinkerCapacityTracker(TimeDelta tracking_window) 
    : tracking_window_(tracking_window),
      estimated_capacity_(DataRate::Zero()),
      last_delay_based_estimate_(DataRate::PlusInfinity()),
      time_last_capacity_udpate_(Timestamp::MinusInfinity()) {}

LinkerCapacityTracker::~LinkerCapacityTracker() = default;

void LinkerCapacityTracker::OnStartingBitrate(DataRate bitrate) {
    // The capacity estimate is still not initialized yet.
    if (time_last_capacity_udpate_.IsInfinite()) {
        PLOG_INFO << "Start bitrate=" << bitrate.bps() << " bps.";
        estimated_capacity_ = bitrate;
    }
}

void LinkerCapacityTracker::OnDelayBasedEstimate(DataRate bitrate, 
                                                 Timestamp at_time) {
    // Update with the delay-based estimate conservatively.
    if (bitrate < last_delay_based_estimate_) {
        PLOG_INFO  << "Delay based bitrate=" << bitrate.bps() 
                   << " bps, current bitrate=" << estimated_capacity_.bps() 
                   << " bps.";
        estimated_capacity_ = std::min(estimated_capacity_, bitrate);
        time_last_capacity_udpate_ = at_time;
    }
    last_delay_based_estimate_ = bitrate;
}

void LinkerCapacityTracker::OnRttBackoffEstimate(DataRate bitrate,
                                                 Timestamp at_time) {
    // Update with the RTT-based backoff conservatively.
    estimated_capacity_ = std::min(estimated_capacity_, bitrate);
    PLOG_INFO << "RTT backoff bitrate=" << bitrate.bps() << " bps.";
    time_last_capacity_udpate_ = at_time;
}

void LinkerCapacityTracker::OnBitrateUpdated(DataRate bitrate, 
                                             Timestamp at_time) {
    // FIXME: Make sure the linker capacity still in a high level?
    if (bitrate > estimated_capacity_) {
        // 距离上一次更新的时间越近，权重值越大。
        TimeDelta delta_since_last_update = at_time - time_last_capacity_udpate_;
        // Calculate the exponential smoothing faction: e^-x = 1 / e^x
        double alpha = delta_since_last_update.IsFinite() ? exp(-(delta_since_last_update / tracking_window_))
                                                         : 0;
        estimated_capacity_ = alpha * estimated_capacity_ + (1 - alpha) * bitrate;
         PLOG_INFO_IF(false) << "capacity bitrate=" << bitrate.bps() 
                             << " bps, updated bitrate=" << estimated_capacity_.bps()
                             << " bps, alpha="<< alpha;
    }
    time_last_capacity_udpate_ = at_time;
}

DataRate LinkerCapacityTracker::estimate() const {
    return estimated_capacity_;
}
    
} // namespace naivertc
