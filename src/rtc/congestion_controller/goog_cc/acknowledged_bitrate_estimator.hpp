#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_ACKNOWLEDGED_BITRATE_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/congestion_controller/network_types.hpp"
#include "rtc/congestion_controller/goog_cc/bitrate_estimator_interface.hpp"

#include <optional>
#include <vector>

namespace naivertc {

class RTC_CPP_EXPORT AcknowledgedBitrateEstimator {
public:
    AcknowledgedBitrateEstimator(std::unique_ptr<BitrateEstimatorInterface> bitrate_estimator);
    ~AcknowledgedBitrateEstimator();

    void set_in_alr(bool in_alr);
    void set_alr_ended_time(Timestamp alr_ended_time);

    void IncomingPacketFeedbackVector(const std::vector<PacketResult>& packet_feedback_vector);

    std::optional<DataRate> Estimate() const;
    std::optional<DataRate> PeekRate() const;

private:
    std::unique_ptr<BitrateEstimatorInterface> bitrate_estimator_;
    bool in_alr_;
    std::optional<Timestamp> alr_ended_time_;
};
    
} // namespace naivertc


#endif