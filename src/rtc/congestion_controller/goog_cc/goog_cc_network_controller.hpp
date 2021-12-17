#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_GOOG_CC_NETWORK_CONTROLLER_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_GOOG_CC_NETWORK_CONTROLLER_H_

#include "base/defines.hpp"
#include "rtc/congestion_controller/network_controller_interface.hpp"

namespace naivertc {

class RTC_CPP_EXPORT GoogCcNetworkController : public NetworkControllerInterface {
public:
    GoogCcNetworkController();
    ~GoogCcNetworkController() override;

    // NetworkControllerInterface
    NetworkControlUpdate OnNetworkAvailability(NetworkAvailability) override;
    NetworkControlUpdate OnNetworkRouteChange(NetworkRouteChange) override;
    NetworkControlUpdate OnProcessInterval(ProcessInterval) override;
    NetworkControlUpdate OnRemoteBitrateReport(RemoteBitrateReport) override;
    NetworkControlUpdate OnRoundTripTimeUpdate(RoundTripTimeUpdate) override;
    NetworkControlUpdate OnSentPacket(SentPacket) override;
    NetworkControlUpdate OnReceivedPacket(ReceivedPacket) override;
    NetworkControlUpdate OnStreamsConfig(StreamsConfig) override;
    NetworkControlUpdate OnTargetBitrateConstraints(TargetBitrateConstraints) override;
    NetworkControlUpdate OnTransportLossReport(TransportLossReport) override;
    NetworkControlUpdate OnTransportPacketsFeedback(TransportPacketsFeedback) override;
    NetworkControlUpdate OnNetworkStateEstimate(NetworkEstimate) override;
};
    
} // namespace naivertc


#endif