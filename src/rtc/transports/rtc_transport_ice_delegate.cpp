#include "rtc/transports/rtc_transport.hpp"

#include <plog/Log.h>

namespace naivertc {

void RtcTransport::InitIceTransport() {
    RTC_RUN_ON(task_queue_);
    if (ice_transport_) {
        return;
    }
    PLOG_VERBOSE << "Init Ice transport";
    auto ice_config = IceTransport::Configuration();
    ice_config.ice_servers = config_.ice_servers;
    ice_config.enable_ice_tcp = config_.enable_ice_tcp;
    ice_config.port_range_begin = config_.port_range_begin;
    ice_config.port_range_end = config_.port_range_end;
#if USE_NICE
    ice_config.proxy_server = config_.proxy_server;
#else
    ice_config.bind_addresses = config_.bind_addresses;
#endif
    ice_transport_.reset(new IceTransport(std::move(ice_config), config_.role, task_queue_));
    
    ice_transport_->OnStateChanged(std::bind(&RtcTransport::OnIceTransportStateChanged, this, std::placeholders::_1));
    ice_transport_->OnGatheringStateChanged(std::bind(&RtcTransport::OnGatheringStateChanged, this, std::placeholders::_1));
    ice_transport_->OnCandidateGathered(std::bind(&RtcTransport::OnCandidateGathered, this, std::placeholders::_1));
    ice_transport_->OnRoleChanged(std::bind(&RtcTransport::OnRoleChanged, this, std::placeholders::_1));

    ice_transport_->Start();  
}

void RtcTransport::OnIceTransportStateChanged(IceTransport::State transport_state) {
    if (transport_state == IceTransport::State::CONNECTED) {
        InitDtlsTransport();
    } else {

    }
}

void RtcTransport::OnGatheringStateChanged(IceTransport::GatheringState gathering_state) {

}

void RtcTransport::OnCandidateGathered(sdp::Candidate candidate) {

}

void RtcTransport::OnRoleChanged(sdp::Role role) {

}

} // namespace naivertc