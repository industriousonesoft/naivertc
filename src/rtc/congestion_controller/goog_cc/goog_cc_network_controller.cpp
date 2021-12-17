#include "rtc/congestion_controller/goog_cc/goog_cc_network_controller.hpp"

namespace naivertc {

GoogCcNetworkController::GoogCcNetworkController() {}

GoogCcNetworkController::~GoogCcNetworkController() {}

NetworkControlUpdate GoogCcNetworkController::OnNetworkAvailability(NetworkAvailability) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnNetworkRouteChange(NetworkRouteChange) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnProcessInterval(ProcessInterval) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnRemoteBitrateReport(RemoteBitrateReport) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnRoundTripTimeUpdate(RoundTripTimeUpdate) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnSentPacket(SentPacket) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnReceivedPacket(ReceivedPacket) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnStreamsConfig(StreamsConfig) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnTargetBitrateConstraints(TargetBitrateConstraints) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnTransportLossReport(TransportLossReport) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnTransportPacketsFeedback(TransportPacketsFeedback) {
    NetworkControlUpdate update;
    return update;
}

NetworkControlUpdate GoogCcNetworkController::OnNetworkStateEstimate(NetworkEstimate) {
    NetworkControlUpdate update;
    return update;
}
    
} // namespace naivertc
