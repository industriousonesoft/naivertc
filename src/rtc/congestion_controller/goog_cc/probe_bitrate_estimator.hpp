#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_PROBE_BITRATE_ESTIMATOR_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_PROBE_BITRATE_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/units/timestamp.hpp"
#include "rtc/base/units/data_rate.hpp"
#include "rtc/congestion_controller/network_types.hpp"

#include <map>
#include <optional>

namespace naivertc {

class RTC_CPP_EXPORT ProbeBitrateEstimator {
public:
    ProbeBitrateEstimator();
    ~ProbeBitrateEstimator();

    // Should be called for every probe packet we receive feedback about.
    // Returns the estimated bitrate if the probe completes a valid cluster.
    std::optional<DataRate> IncomingProbePacket(const PacketResult& packet_feedback);

    // Return the latest estimated bitrate and will reset it after reading by default.
    std::optional<DataRate> Estimate(bool reset_after_reading = true);

private:
    struct AggregatedCluster {
        int num_probes = 0;
        Timestamp first_send_time = Timestamp::PlusInfinity();
        Timestamp last_send_time = Timestamp::MinusInfinity();
        Timestamp first_recv_time = Timestamp::PlusInfinity();
        Timestamp last_recv_time = Timestamp::MinusInfinity();
        size_t last_send_size = 0;
        size_t first_recv_size = 0;
        size_t accumulated_size = 0;
    };

    // Erases old cluster data that was seen before |timestamp|.
    void EraseOldCluster(Timestamp timestamp);

private:
    std::map<int, AggregatedCluster> clusters_;
    std::optional<DataRate> estimated_bitrate_;
};
    
} // namespace naivertc


#endif