#include "pc/transports/ice_transport.hpp"

#include <plog/Log.h>

#include <memory>

namespace naivertc {

IceTransport::IceTransport(const Configuration& config) 
    : Transport(nullptr), 
    juice_agent_(nullptr, nullptr), 
    curr_mid_("0"),
    role_(sdp::Role::ACT_PASS) {
    
    if (config.enable_ice_tcp) {
        PLOG_WARNING << "ICE-TCP is not supported with libjuice.";
    }

    InitJuice(config);
}

sdp::Role IceTransport::role() const {
    return role_;
}

void IceTransport::GatheringLocalCandidate(std::string mid) {
    curr_mid_ = std::move(mid);
    OnJuiceGetheringStateChanged(GatheringState::NEW);

    if (juice_gather_candidates(juice_agent_.get()) < 0) {
        throw std::runtime_error("Failed to gather local ICE candidate.");
    }
}

bool IceTransport::AddRemoteCandidate(const Candidate& candidate) {
    if (!candidate.isResolved()) {
        PLOG_WARNING << "Don't try to pass unresolved candidates for more safety.";
        return false;
    }
    // juice_send_diffserv
    return juice_add_remote_candidate(juice_agent_.get(), std::string(candidate).c_str()) >= 0;
}

std::optional<std::string> IceTransport::GetLocalAddress() const {
    char buffer[JUICE_MAX_ADDRESS_STRING_LEN];
    if (juice_get_selected_addresses(juice_agent_.get(), buffer, JUICE_MAX_ADDRESS_STRING_LEN, NULL, 0) == 0) {
        return std::make_optional(std::string(buffer));
    }
    return std::nullopt;
}

std::optional<std::string> IceTransport::GetRemoteAddress() const {
    char buffer[JUICE_MAX_ADDRESS_STRING_LEN];
    if (juice_get_selected_addresses(juice_agent_.get(), NULL, 0, buffer, JUICE_MAX_ADDRESS_STRING_LEN) == 0) {
        return std::make_optional(std::string(buffer));
    }
    return std::nullopt;
}
sdp::SessionDescription IceTransport::GetLocalDescription(sdp::Type type) const {
    char sdp[JUICE_MAX_SDP_STRING_LEN];
    if (juice_get_local_description(juice_agent_.get(), sdp, JUICE_MAX_SDP_STRING_LEN) < 0) {
        throw std::runtime_error("Failed to generate local SDP.");
    }
    // RFC 5763: The endpoint that is the offer MUST use the setup attribute value of setup::actpass
    // See https://tools.ietf.org/html/rfc5763#section-5
    return sdp::SessionDescription(std::string(sdp), type, type == sdp::Type::OFFER ? sdp::Role::ACT_PASS : role_);
}

void IceTransport::SetRemoteDescription(const sdp::SessionDescription& description) {
    if (role_ == sdp::Role::ACT_PASS) {
        role_ = description.role() == sdp::Role::ACTIVE ? sdp::Role::PASSIVE : sdp::Role::PASSIVE;
    }
    if (role_ == description.role()) {
        throw std::logic_error("Incompatible roles with remote description.");
    }
    curr_mid_ = description.bundle_id();
    if (juice_set_remote_description(juice_agent_.get(), description.GenerateSDP("\r\n", true /* sdp without audio or video media lines */).c_str())) {
        throw std::runtime_error("Failed to parse ICE settings from remote SDP.");
    }
}

// Override 
void IceTransport::Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) {
    send_queue_.Post([this, packet, callback](){
        // A filter for valid packet and state
        auto state = this->state();
        if (!packet || state != State::CONNECTED || state != State::COMPLETED) {
            if (callback) {
                callback(false);
            }
            return;
        }
        this->Outgoing(packet, callback);
    });
}

void IceTransport::Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback) {
    // Explicit Congestion Notification takes the least-significant 2 bits of the DS field.
    int ds = int(out_packet->dscp() << 2);
    bool bRet = juice_send_diffserv(juice_agent_.get(), out_packet->data(), out_packet->size(), ds) >= 0;
    if (callback) {
        callback(bRet);
    }
}

} // end of naivertc