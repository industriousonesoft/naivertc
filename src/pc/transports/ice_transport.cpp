#include "pc/transports/ice_transport.hpp"

#include <plog/Log.h>

#include <memory>

namespace naivertc {

using namespace std::chrono_literals;

IceTransport::IceTransport(const RtcConfiguration& config) 
    : Transport(nullptr), 
    curr_mid_("0"),
    role_(sdp::Role::ACT_PASS) {
#if !USE_NICE
    if (config.enable_ice_tcp) {
        PLOG_WARNING << "ICE-TCP is not supported with libjuice.";
    }
    InitJuice(std::move(config));
#else 
    InitNice(std::move(config));
#endif
}

IceTransport::~IceTransport() {
    Stop();
}

sdp::Role IceTransport::role() const {
    return role_;
}

void IceTransport::Stop(StopedCallback callback) {
#if !USE_NICE
    Transport::Stop(callback);
#else
    Transport::Stop([this, callback](std::optional<const std::exception> exp){
        if (timeout_id_ > 0) {
            g_source_remove(timeout_id_);
            timeout_id_ = 0;
        }

        PLOG_DEBUG << "Stopping ICE thread";
        nice_agent_attach_recv(nice_agent_.get(), stream_id_, component_id_, g_main_loop_get_context(main_loop_.get()), NULL, NULL);
        nice_agent_remove_stream(nice_agent_.get(), stream_id_);
        g_main_loop_quit(main_loop_.get());
        main_loop_thread_.join();
        if (callback) {
            callback(std::move(exp));
        }
    });
#endif
}

void IceTransport::GatherLocalCandidate(std::string mid) {
    curr_mid_ = std::move(mid);
    // Change state now as candidates start to gather can be synchronous
    OnGetheringStateChanged(GatheringState::GATHERING);

#if !USE_NICE
    if (juice_gather_candidates(juice_agent_.get()) < 0) {
        throw std::runtime_error("Failed to gather local ICE candidate");
    }
#else
    if (nice_agent_gather_candidates(nice_agent_.get(), stream_id_) == false) {
        throw std::runtime_error("Failed to gather local ICE candidates");
    }
#endif
}

bool IceTransport::AddRemoteCandidate(const Candidate& candidate) {
    if (!candidate.isResolved()) {
        PLOG_WARNING << "Don't try to pass unresolved candidates for more safety.";
        return false;
    }
#if !USE_NICE
    // juice_send_diffserv
    return juice_add_remote_candidate(juice_agent_.get(), std::string(candidate).c_str()) >= 0;
#else
    // The candidate string must start with "a=candidate" and it must not end with 
    // a newline or whitespace, else libnice will reject it.
    auto candidate_sdp = candidate.sdp_line();
    NiceCandidate* nice_candidate = nice_agent_parse_remote_candidate_sdp(nice_agent_.get(), stream_id_, candidate_sdp.c_str());
    if (!nice_candidate) {
        PLOG_WARNING << "libnice rejected ICE candidate: " << candidate_sdp;
        return false;
    }
    GSList* list = g_slist_append(nullptr, nice_candidate);
    int ret = nice_agent_set_remote_candidates(nice_agent_.get(), stream_id_, component_id_, list);
    g_slist_free_full(list, reinterpret_cast<GDestroyNotify>(nice_candidate_free));
    return ret > 0;
#endif
}

std::optional<std::string> IceTransport::GetLocalAddress() const {
#if !USE_NICE
    char buffer[JUICE_MAX_ADDRESS_STRING_LEN];
    if (juice_get_selected_addresses(juice_agent_.get(), buffer, JUICE_MAX_ADDRESS_STRING_LEN, NULL, 0) == 0) {
        return std::make_optional(std::string(buffer));
    }
#else
    NiceCandidate* local = nullptr;
    NiceCandidate* remote = nullptr;
    if (nice_agent_get_selected_pair(nice_agent_.get(), stream_id_, component_id_, &local, &remote)) {
        return std::make_optional(NiceAddressToString(local->addr));
    }
#endif
    return std::nullopt;
}

std::optional<std::string> IceTransport::GetRemoteAddress() const {
#if !USE_NICE
    char buffer[JUICE_MAX_ADDRESS_STRING_LEN];
    if (juice_get_selected_addresses(juice_agent_.get(), NULL, 0, buffer, JUICE_MAX_ADDRESS_STRING_LEN) == 0) {
        return std::make_optional(std::string(buffer));
    }
#else 
    NiceCandidate* local = nullptr;
    NiceCandidate* remote = nullptr;
    if (nice_agent_get_selected_pair(nice_agent_.get(), stream_id_, component_id_, &local, &remote)) {
        return std::make_optional(NiceAddressToString(remote->addr));
    }
#endif
    return std::nullopt;
}
sdp::SessionDescription IceTransport::GetLocalDescription(sdp::Type type) const {
#if !USE_NICE
    char sdp[JUICE_MAX_SDP_STRING_LEN];
    if (juice_get_local_description(juice_agent_.get(), sdp, JUICE_MAX_SDP_STRING_LEN) < 0) {
        throw std::runtime_error("Failed to generate local SDP.");
    }
#else
    // RFC 8445: The initiating agent that started the ICE porcess MUST take the controlling 
    // role, and the other MUST take the controlled role.
    g_object_set(G_OBJECT(nice_agent_.get()), "controlling-mode", type == sdp::Type::OFFER ? TRUE : FALSE, nullptr);

    std::unique_ptr<gchar[], void(*)(void *)> sdp(nice_agent_generate_local_sdp(nice_agent_.get()), g_free);
#endif
    // RFC 5763: The endpoint that is the offer MUST use the setup attribute value of setup::actpass
    // See https://tools.ietf.org/html/rfc5763#section-5
    return sdp::SessionDescription(std::string(sdp.get()), type, type == sdp::Type::OFFER ? sdp::Role::ACT_PASS : role_);
}

void IceTransport::SetRemoteDescription(const sdp::SessionDescription& remote_sdp) {
    if (role_ == sdp::Role::ACT_PASS) {
        role_ = remote_sdp.role() == sdp::Role::ACTIVE ? sdp::Role::PASSIVE : sdp::Role::ACTIVE;
    }
    if (role_ == remote_sdp.role()) {
        throw std::logic_error("Incompatible roles with remote description.");
    }
    curr_mid_ = remote_sdp.bundle_id();
    int ret = 0;
    // sdp without audio or video media stream
    const bool application_only = true;
#if !USE_NICE
    auto eol = "\r\n";
    ret = juice_set_remote_description(juice_agent_.get(), remote_sdp.GenerateSDP(eol, application_only).c_str())
#else
    trickle_timeout_ = 30s;
    // libnice expects "\n" as end of line
    auto eol = "\n";
    auto remote_sdp_string = remote_sdp.GenerateSDP(eol, application_only);
    PLOG_DEBUG << "Nice ready to set remote sdp: \n" << remote_sdp_string;
    // warning: the applicaion sdp line 'm=application' MUST be in front of 'a=ice-ufrag' and 'a=ice-pwd' or contains the both attributes, 
    // otherwise it will be faild to parse ICE settings from sdp.
    ret = nice_agent_parse_remote_sdp(nice_agent_.get(), remote_sdp_string.c_str());
#endif
    if (ret < 0) {
        throw std::runtime_error("Failed to parse ICE settings from remote SDP");
    }
}

std::pair<std::optional<Candidate>, std::optional<Candidate>> IceTransport::GetSelectedCandidatePair() {
    std::optional<Candidate> selected_local_candidate = std::nullopt;
    std::optional<Candidate> selected_remote_candidate = std::nullopt;
#if !USE_NICE
    char local_candidate_sdp[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
	char remote_candidate_sdp[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
    if (juice_get_selected_candidates(mAgent.get(), local_candidate_sdp, JUICE_MAX_CANDIDATE_SDP_STRING_LEN,
	                                  remote_candidate_sdp, JUICE_MAX_CANDIDATE_SDP_STRING_LEN) == 0) {
        auto local_candidate = Candidate(local_candidate_sdp, curr_mid_);
        local_candidate.Resolve(Candidate::ResolveMode::SIMPLE);
		selected_local_candidate= std::make_optional(std::move(local_candidate));

        auto remote_candidate = Candidate(remote_candidate_sdp, curr_mid_);
        remote_candidate.Resolve(Candidate::ResolveMode::SIMPLE);
		selected_remote_candidate = std::make_optional(std::move(remote_candidate));
	}
#else
    NiceCandidate *nice_local_candidate, *nice_remote_candidate;
    if (!nice_agent_get_selected_pair(nice_agent_.get(), stream_id_, component_id_, &nice_local_candidate, &nice_remote_candidate)) {
        return std::make_pair(std::nullopt, std::nullopt);
    }
    gchar* local_candidate_sdp = nice_agent_generate_local_candidate_sdp(nice_agent_.get(), nice_local_candidate);
    if (local_candidate_sdp) {
        auto local_candidate = Candidate(local_candidate_sdp, curr_mid_);
        local_candidate.Resolve(Candidate::ResolveMode::SIMPLE);
        selected_local_candidate = std::make_optional(std::move(local_candidate));
    }

    gchar* remote_candidate_sdp = nice_agent_generate_local_candidate_sdp(nice_agent_.get(), nice_remote_candidate);
    if (remote_candidate_sdp) {
        auto remote_candidate = Candidate(remote_candidate_sdp, curr_mid_);
        remote_candidate.Resolve(Candidate::ResolveMode::SIMPLE);
        selected_remote_candidate = std::make_optional(std::move(remote_candidate));
    }

    g_free(local_candidate_sdp);
    g_free(remote_candidate_sdp);

#endif
    return std::make_pair(selected_local_candidate, selected_remote_candidate);
}

// State Callbacks
void IceTransport::OnStateChanged(State state) {
    Transport::UpdateState(state);
}

void IceTransport::OnGetheringStateChanged(GatheringState state) {
    task_queue_.Post([this, state](){
        if (this->gathering_state_.exchange(state) != state) {
            this->SignalGatheringStateChanged(state);
        }
    });
}

void IceTransport::OnCandidateGathered(const char* sdp) {
    task_queue_.Post([this, sdp = std::move(sdp)](){
        this->SignalCandidateGathered(std::move(Candidate(sdp, this->curr_mid_)));
    });
}

void IceTransport::OnDataReceived(const char* data, size_t size) {
    task_queue_.Post([this, data = std::move(data), size](){
        try {
            PLOG_VERBOSE << "Incoming size: " << size;
            auto packet = Packet::Create(data, size);
            HandleIncomingPacket(packet);
        } catch(const std::exception &e) {
            PLOG_WARNING << e.what();
        }
    });
}

// Override 
void IceTransport::Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) {
    task_queue_.Post([this, packet, callback](){
        // A filter for valid packet and state
        auto state = this->state();
        if (!packet || (state != State::CONNECTED && state != State::COMPLETED)) {
            if (callback) {
                callback(false);
            }
            return;
        }
        this->Outgoing(packet, callback);
    });
}

void IceTransport::Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback) {
    bool bRet = false;
#if !USE_NICE
    // Explicit Congestion Notification takes the least-significant 2 bits of the DS field.
    int ds = int(out_packet->dscp() << 2);
    bRet = juice_send_diffserv(juice_agent_.get(), out_packet->data(), out_packet->size(), ds) >= 0;
#else
    if (outgoing_dscp_ != out_packet->dscp()) {
        outgoing_dscp_ = out_packet->dscp();
        // Explicit Congestion Notification takes the least-significant 2 bits of the DS field
        int ds = int(outgoing_dscp_ << 2);
        // ToS is the lagacy name for DS
        nice_agent_set_stream_tos(nice_agent_.get(), stream_id_, ds);
    }
    bRet = nice_agent_send(nice_agent_.get(), stream_id_, component_id_, out_packet->size(), reinterpret_cast<const char*>(out_packet->data())) >= 0;
#endif
    if (callback) {
        callback(bRet);
    }
}

void IceTransport::ParseIceSettingFromSDP(NiceAgent *agent, const gchar *sdp) {
    gchar **sdp_lines = NULL;
    gint ret = 0;

    sdp_lines = g_strsplit (sdp, "\n", 0);
    for (gint i = 0; sdp_lines && sdp_lines[i]; i++) {

        if (g_str_has_prefix (sdp_lines[i], "m=")) {
            PLOG_DEBUG << "has app";
        } else if (g_str_has_prefix (sdp_lines[i], "a=ice-ufrag:")) {
            PLOG_DEBUG << "has ice-ufrag";
        } else if (g_str_has_prefix (sdp_lines[i], "a=ice-pwd:")) {
            PLOG_DEBUG << "has ice-pwd";
        }
  }

  if (sdp_lines)
    g_strfreev(sdp_lines);
}

} // namespace naivertc