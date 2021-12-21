#ifndef _RTC_CONGESTION_CONTROLLER_NETWORK_CONTROLLER_INTERFACE_H_
#define _RTC_CONGESTION_CONTROLLER_NETWORK_CONTROLLER_INTERFACE_H_

#include "base/defines.hpp"
#include "rtc/congestion_controller/network_types.hpp"

namespace naivertc {

class RTC_CPP_EXPORT NetworkControllerInterface {
public:
    struct Configuration {
        // The initial constraints to start with.
        TargetBitrateConstraints constraints;
        // The stream specific configuration.
        StreamsConfig stream_based_config;
    };
public:
    virtual ~NetworkControllerInterface() = default;

    // Called when network availability changes.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnNetworkAvailability(NetworkAvailability) = 0;
    // Called when the receiving or sending endpoint changes address.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnNetworkRouteChange(NetworkRouteChange) = 0;
    // Called periodically with a periodicy.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnProcessInterval(ProcessInterval) = 0;
    // Called when the bitrate calculated by the remote is received.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnRemoteBitrateReport(RemoteBitrateReport) = 0;
    // Called when the RTT has been calculated by protocol sepcific mechanisms.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnRoundTripTimeUpdate(RoundTripTimeUpdate) = 0;
    // Called when a packet is sent on the network.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnSentPacket(SentPacket) = 0;
    // Called when a packet is received from the remote.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnReceivedPacket(ReceivedPacket) = 0;
    // Called when the stream specific configuration has been updated.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnStreamsConfig(StreamsConfig) = 0;
    // Called when target transfer rate constraints has been changed.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnTargetBitrateConstraints(TargetBitrateConstraints) = 0;
    // Called when a protocol specific calculation of packet loss has been made.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnTransportLossReport(TransportLossReport) = 0;
    // Called with per packet feedback regarding receive time.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnTransportPacketsFeedback(TransportPacketsFeedback) = 0;
    // Called with network state estimate updates.
    RTC_MUST_USE_RESULT virtual NetworkControlUpdate OnNetworkStateEstimate(NetworkEstimate) = 0;
};
    
} // namespace naivertc


#endif