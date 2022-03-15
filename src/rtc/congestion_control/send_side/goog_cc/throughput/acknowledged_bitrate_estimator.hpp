#ifndef _RTC_CONGESTION_CONTROL_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_
#define _RTC_CONGESTION_CONTROL_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/congestion_control/base/network_types.hpp"
#include "rtc/congestion_control/send_side/goog_cc/throughput/throughput_estimator.hpp"

#include <optional>
#include <vector>

namespace naivertc {

// This class estimate the acknowledged bitrate based on 
// the packets acknowledged by receiver
class AcknowledgedBitrateEstimator {
public:
    static std::unique_ptr<AcknowledgedBitrateEstimator> Create(ThroughputEstimator::Configuration config);
public:
    AcknowledgedBitrateEstimator(std::unique_ptr<ThroughputEstimator> bitrate_estimator);
    ~AcknowledgedBitrateEstimator();

    // Indicates if we are in Application Limit Region or not.
    void set_in_alr(bool in_alr);
    // The time to end Application Limit Region.
    void set_alr_ended_time(Timestamp alr_ended_time);

    void IncomingPacketFeedbacks(const std::vector<PacketResult>& packet_feedbacks);

    std::optional<DataRate> Estimate() const;
    std::optional<DataRate> PeekRate() const;

private:
    std::unique_ptr<ThroughputEstimator> throughput_estimator_;
    bool in_alr_;
    std::optional<Timestamp> alr_ended_time_;
};
    
} // namespace naivertc


#endif