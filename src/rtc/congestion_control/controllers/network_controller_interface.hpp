#ifndef _RTC_CONGESTION_CONTROL_NETWORK_CONTROLLER_INTERFACE_H_
#define _RTC_CONGESTION_CONTROL_NETWORK_CONTROLLER_INTERFACE_H_

#include "base/defines.hpp"
#include "rtc/congestion_control/base/network_types.hpp"
#include "rtc/congestion_control/base/rate_control_settings.hpp"

namespace naivertc {

class Clock;

class NetworkControllerInterface {
public:
    struct Configuration {
        Clock* clock = nullptr;
        // The initial constraints to start with.
        TargetBitrateConstraints constraints;
        // The stream specific configuration.
        StreamsConfig stream_based_config;
        // Bitrate control settings.
        RateControlSettings rate_control_settings;
    };
public:
    virtual ~NetworkControllerInterface() = default;

    // Called when network availability changes.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnNetworkAvailability(const NetworkAvailability&) = 0;
    // Called when the receiving or sending endpoint changes address.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnNetworkRouteChange(const NetworkRouteChange&) = 0;
    // Called periodically with a periodicy.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnPeriodicUpdate(const PeriodicUpdate&) = 0;
    // Called when the bitrate calculated by the remote is received.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnRemoteBitrateUpdated(DataRate bitrate, Timestamp receive_time) = 0;
    // Called when the RTT has been calculated by protocol sepcific mechanisms.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnRttUpdated(TimeDelta rtt, Timestamp receive_time) = 0;
    // Called when a packet is sent on the network.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnSentPacket(const SentPacket&) = 0;
    // Called when a packet is received from the remote.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnReceivedPacket(const ReceivedPacket&) = 0;
    // Called when the stream specific configuration has been updated.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnStreamsConfig(const StreamsConfig&) = 0;
    // Called when target transfer rate constraints has been changed.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnTargetBitrateConstraints(const TargetBitrateConstraints&) = 0;
    // Called when a protocol specific calculation of packet loss has been made.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnTransportLostReport(const TransportLossReport&) = 0;
    // Called with per packet feedback regarding receive time.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnTransportPacketsFeedback(const TransportPacketsFeedback&) = 0;
    // Called with network state estimate updates.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnNetworkStateEstimate(const NetworkEstimate&) = 0;
};
    
} // namespace naivertc


#endif