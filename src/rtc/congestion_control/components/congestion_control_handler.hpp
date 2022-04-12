#ifndef _RTC_CONGESTION_CONTROL_COMPONENTS_CONGESTION_CONTROL_HANDLER_H_
#define _RTC_CONGESTION_CONTROL_COMPONENTS_CONGESTION_CONTROL_HANDLER_H_

#include "rtc/congestion_control/base/bwe_types.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"

#include <optional>

namespace naivertc {

class CongestionControlHandler {
public:
    CongestionControlHandler(bool enable_pacer_emergency_stop = true);
    ~CongestionControlHandler();

    void OnNetworkAvailability(bool network_available);
    void OnPacerExpectedQueueTime(TimeDelta expected_queue_time);
    void OnTargetTransferBitrate(TargetTransferBitrate target_bitrate);

    std::optional<TargetTransferBitrate> GetUpdate();

private:
    bool BelongsToNewReport(const TargetTransferBitrate& new_outgoing) const;

private:
    SequenceChecker sequence_checker_;
    const bool enable_pacer_emergency_stop_;
    std::optional<TargetTransferBitrate> last_incoming_;
    std::optional<TargetTransferBitrate> last_reported_;
    bool network_available_ = true;
    TimeDelta pacer_expected_queue_time_ = TimeDelta::Zero();

};
    
} // namespace naivertc


#endif