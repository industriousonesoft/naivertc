#include "rtc/congestion_controller/components/linker_capacity_tracker.hpp"

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
        estimated_capacity_ = bitrate;
    }
}

void LinkerCapacityTracker::OnDelayBasedEstimate(DataRate bitrate, 
                                                 Timestamp at_time) {
    if (bitrate < last_delay_based_estimate_) {
        estimated_capacity_ = std::min(estimated_capacity_, bitrate);
        time_last_capacity_udpate_ = at_time;
    }
    last_delay_based_estimate_ = bitrate;
}

void LinkerCapacityTracker::OnRttBasedEstimate(DataRate bitrate,
                                               Timestamp at_time) {
    estimated_capacity_ = std::min(estimated_capacity_, bitrate);
    time_last_capacity_udpate_ = at_time;
}

void LinkerCapacityTracker::Update(DataRate bitrate, 
                                   Timestamp at_time) {
    if (bitrate > estimated_capacity_) {
        TimeDelta elapsed_time = at_time - time_last_capacity_udpate_;
        // Calculate the exponential smoothing faction: e^-x = 1 / e^x
        double alpha = elapsed_time.IsFinite() ? exp(-(elapsed_time / tracking_window_))
                                               : 0;
        estimated_capacity_ = alpha * estimated_capacity_ + (1 - alpha) * bitrate;
    }
    time_last_capacity_udpate_ = at_time;
}

DataRate LinkerCapacityTracker::estimate() const {
    return estimated_capacity_;
}
    
} // namespace naivertc
