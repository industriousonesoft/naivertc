#include "rtc/transports/ice_transport.hpp"

#include <plog/Log.h>

#include <memory>
#include <sstream>

namespace naivertc {

using namespace std::chrono_literals;

IceTransport::IceTransport(const RtcConfiguration& config, std::shared_ptr<TaskQueue> task_queue) 
    : Transport(std::weak_ptr<Transport>(), std::move(task_queue)), 
      curr_mid_("0"),
      role_(sdp::Role::ACT_PASS) {
#if !USE_NICE
    if (config.enable_ice_tcp) {
        PLOG_WARNING << "ICE-TCP is not supported with libjuice.";
    }
    InitJuice(config);
#else 
    InitNice(config);
#endif
}

IceTransport::~IceTransport() {
    Stop();
}

sdp::Role IceTransport::role() const {
    return task_queue_->Sync<sdp::Role>([this](){
        return role_;
    });
}

std::exception_ptr IceTransport::last_exception() const {
    return task_queue_->Sync<std::exception_ptr>([this](){
        return last_exception_;
    });
}

void IceTransport::OnCandidateGathered(CandidateGatheredCallback callback) {
    task_queue_->Async([this, callback](){
        candidate_gathered_callback_ = std::move(callback);
    });
}

void IceTransport::OnGatheringStateChanged(GatheringStateChangedCallback callback) {
    task_queue_->Async([this, callback](){
        gathering_state_changed_callback_ = std::move(callback);
    });
}

void IceTransport::OnRoleChanged(RoleChangedCallback callback) {
    task_queue_->Async([this, callback](){
        role_changed_callback_ = std::move(callback);
    });
}

bool IceTransport::Start() {
    return task_queue_->Sync<bool>([this](){
        if (is_stoped_) {
            RegisterIncoming();
            is_stoped_= false;
        }
        return true;
    });
}

bool IceTransport::Stop() {
    return task_queue_->Sync<bool>([this](){
        if (!is_stoped_) {
#if USE_NICE
            if (timeout_id_ > 0) {
                g_source_remove(timeout_id_);
                timeout_id_ = 0;
            }
            PLOG_DEBUG << "Stopping NICE ICE thread";
            nice_agent_attach_recv(nice_agent_.get(), stream_id_, component_id_, g_main_loop_get_context(main_loop_.get()), NULL, NULL);
            nice_agent_remove_stream(nice_agent_.get(), stream_id_);
            g_main_loop_quit(main_loop_.get());
            main_loop_thread_.join();
#endif
            DeregisterIncoming();
            is_stoped_ = true;
        }
        return true;
    });
}

void IceTransport::StartToGatherLocalCandidate(std::string mid) {
    task_queue_->Async([this, mid=std::move(mid)](){
        try {
            curr_mid_ = std::move(mid);
            // Change state now as candidates start to gather can be synchronous
            UpdateGatheringState(GatheringState::GATHERING);

        #if !USE_NICE
            if (juice_gather_candidates(juice_agent_.get()) < 0) {
                throw std::runtime_error("Failed to gather local ICE candidate");
            }
        #else
            if (nice_agent_gather_candidates(nice_agent_.get(), stream_id_) == false) {
                throw std::runtime_error("Failed to gather local ICE candidates");
            }
        #endif
        }catch (...) {
            last_exception_ = std::current_exception();
        }    
    });
}

void IceTransport::AddRemoteCandidate(const sdp::Candidate candidate) {
    task_queue_->Async([this, candidate=std::move(candidate)](){
        try {
            // Don't try to pass unresolved candidates for more safety.
            if (!candidate.isResolved()) {
                return;
            }
            bool bRet = false;
            auto candidate_sdp = candidate.sdp_line();
        #if !USE_NICE
            // juice_send_diffserv
            bRet = juice_add_remote_candidate(juice_agent_.get(), candidate_sdp.c_str()) >= 0;
        #else
            // The candidate string must start with "a=candidate" and it must not end with 
            // a newline or whitespace, else libnice will reject it.
            NiceCandidate* nice_candidate = nice_agent_parse_remote_candidate_sdp(nice_agent_.get(), stream_id_, candidate_sdp.c_str());
            if (!nice_candidate) {
                // throw std::runtime_error("Failed to parse remote candidate: " + candidate_sdp);
                PLOG_WARNING << "Rejected ICE candidate: " << candidate_sdp;
                return;
            }
            GSList* list = g_slist_append(nullptr, nice_candidate);
            bRet = nice_agent_set_remote_candidates(nice_agent_.get(), stream_id_, component_id_, list) > 0;
            g_slist_free_full(list, reinterpret_cast<GDestroyNotify>(nice_candidate_free));
        #endif
            if (!bRet) {
                // throw std::runtime_error("Failed to add remote candidate: " + candidate_sdp);
                PLOG_WARNING << "Failed to add remote candidate: " << candidate_sdp;
            }  
        }catch (...) {
            last_exception_ = std::current_exception();
        }
    });
}

std::optional<std::string> IceTransport::GetLocalAddress() const {
    return task_queue_->Sync<std::optional<std::string>>([this](){
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
        return std::optional<std::string>(std::nullopt);
    });
}

std::optional<std::string> IceTransport::GetRemoteAddress() const {
    return task_queue_->Sync<std::optional<std::string>>([this](){
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
        return std::optional<std::string>(std::nullopt);
    });
}

IceTransport::Description IceTransport::GetLocalDescription(sdp::Type type) const {
    return task_queue_->Sync<IceTransport::Description>([this, type](){
        // RFC 5763: The endpoint that is the offer MUST use the setup attribute value of setup::actpass
        // See https://tools.ietf.org/html/rfc5763#section-5
        auto role = type == sdp::Type::OFFER ? sdp::Role::ACT_PASS : role_;
    #if !USE_NICE
        char sdp_buffer[JUICE_MAX_SDP_STRING_LEN];
        if (juice_get_local_description(juice_agent_.get(), sdp_buffer, JUICE_MAX_SDP_STRING_LEN) < 0) {
            return Description(type, role);
        }
    #else
        // RFC 8445: The initiating agent that started the ICE porcess MUST take the controlling 
        // role, and the other MUST take the controlled role.
        g_object_set(G_OBJECT(nice_agent_.get()), "controlling-mode", type == sdp::Type::OFFER ? TRUE : FALSE, nullptr);
        auto sdp = nice_agent_generate_local_sdp(nice_agent_.get());
        if (sdp == NULL) {
            return Description(type, role);
        }
        std::unique_ptr<gchar[], void(*)(void *)> sdp_buffer(std::move(sdp), g_free);
    #endif
        return Description::Parse(std::string(sdp_buffer.get()), type, role);
    });
}

void IceTransport::SetRemoteDescription(const Description remote_sdp) {
    task_queue_->Async([this, remote_sdp=std::move(remote_sdp)](){
        try {
            NegotiateRole(remote_sdp.role());
            int ret = 0;
        #if !USE_NICE
            auto eol = "\r\n";
            ret = juice_set_remote_description(juice_agent_.get(), remote_sdp.GenerateSDP(eol).c_str())
        #else
            trickle_timeout_ = 30s;
            // libnice expects "\n" as end of line
            auto eol = "\n";
            auto remote_sdp_string = remote_sdp.GenerateSDP(eol);
            PLOG_DEBUG << "Nice ready to set remote sdp: \n" << remote_sdp_string;
            // warning: the applicaion sdp line 'm=application' MUST be in front of 'a=ice-ufrag' and 'a=ice-pwd' or contains the both attributes, 
            // otherwise it will be faild to parse ICE settings from sdp.
            ret = nice_agent_parse_remote_sdp(nice_agent_.get(), remote_sdp_string.c_str());
        #endif
            if (ret < 0) {
                throw std::runtime_error("Failed to parse ICE settings from remote SDP");
            }
        }catch(...) {
            last_exception_ = std::current_exception();
        }
    });
}

IceTransport::CandidatePair IceTransport::GetSelectedCandidatePair() const {
    return task_queue_->Sync<CandidatePair>([this](){
        std::optional<sdp::Candidate> selected_local_candidate = std::nullopt;
        std::optional<sdp::Candidate> selected_remote_candidate = std::nullopt;
    #if !USE_NICE
        char local_candidate_sdp[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
        char remote_candidate_sdp[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
        if (juice_get_selected_candidates(mAgent.get(), local_candidate_sdp, JUICE_MAX_CANDIDATE_SDP_STRING_LEN,
                                        remote_candidate_sdp, JUICE_MAX_CANDIDATE_SDP_STRING_LEN) == 0) {
            auto local_candidate = sdp::Candidate(local_candidate_sdp, curr_mid_);
            local_candidate.Resolve(sdp::Candidate::ResolveMode::SIMPLE);
            selected_local_candidate= std::make_optional(std::move(local_candidate));

            auto remote_candidate = sdp::Candidate(remote_candidate_sdp, curr_mid_);
            remote_candidate.Resolve(sdp::Candidate::ResolveMode::SIMPLE);
            selected_remote_candidate = std::make_optional(std::move(remote_candidate));
        }
    #else
        NiceCandidate *nice_local_candidate, *nice_remote_candidate;
        if (!nice_agent_get_selected_pair(nice_agent_.get(), stream_id_, component_id_, &nice_local_candidate, &nice_remote_candidate)) {
            return std::make_pair(selected_local_candidate, selected_remote_candidate);
        }
        gchar* local_candidate_sdp = nice_agent_generate_local_candidate_sdp(nice_agent_.get(), nice_local_candidate);
        if (local_candidate_sdp) {
            auto local_candidate = sdp::Candidate(local_candidate_sdp, curr_mid_);
            local_candidate.Resolve(sdp::Candidate::ResolveMode::SIMPLE);
            selected_local_candidate = std::make_optional(std::move(local_candidate));
        }

        gchar* remote_candidate_sdp = nice_agent_generate_local_candidate_sdp(nice_agent_.get(), nice_remote_candidate);
        if (remote_candidate_sdp) {
            auto remote_candidate = sdp::Candidate(remote_candidate_sdp, curr_mid_);
            remote_candidate.Resolve(sdp::Candidate::ResolveMode::SIMPLE);
            selected_remote_candidate = std::make_optional(std::move(remote_candidate));
        }

        g_free(local_candidate_sdp);
        g_free(remote_candidate_sdp);

    #endif
        return std::make_pair(selected_local_candidate, selected_remote_candidate);
    });
}

int IceTransport::Send(CopyOnWriteBuffer packet, const PacketOptions& options) {
    return task_queue_->Sync<int>([this, packet=std::move(packet), &options]() mutable {
        if (packet.empty() || (state_ != State::CONNECTED && state_ != State::COMPLETED)) {
            return -1;
        }
        return Outgoing(std::move(packet), options);
    });
}

// Private methods
void IceTransport::NegotiateRole(sdp::Role remote_role) {
    // If we can act the both DTLS server and client, to decide local role according to remote role.
    if (role_ == sdp::Role::ACT_PASS) {
        role_ = remote_role == sdp::Role::ACTIVE ? sdp::Role::PASSIVE : sdp::Role::ACTIVE;
        if (role_changed_callback_) {
            role_changed_callback_(role_);
        }
    }
    // To make the role of local is not same as the remote after negotiation
    if (role_ == remote_role) {
        throw std::logic_error("Incompatible roles with remote description.");
    }
}

void IceTransport::UpdateGatheringState(GatheringState state) {
    task_queue_->Async([this, state](){
        if (gathering_state_.exchange(state) != state) {
            if (gathering_state_changed_callback_) {
                gathering_state_changed_callback_(state);
            }
        }
    });
}

void IceTransport::ProcessGatheredCandidate(const char* sdp) {
    task_queue_->Async([this, sdp = std::move(sdp)](){
        if (candidate_gathered_callback_) {
            candidate_gathered_callback_(std::move(sdp::Candidate(sdp, curr_mid_)));
        }
    });
}

void IceTransport::ProcessReceivedData(const char* data, size_t size) {
    task_queue_->Async([this, data = std::move(data), size](){
        try {
            // PLOG_VERBOSE << "Incoming ICE packet size: " << size;
            auto bytes = reinterpret_cast<const uint8_t*>(data);
            Incoming(CopyOnWriteBuffer(bytes, size));
        } catch(const std::exception &e) {
            PLOG_WARNING << e.what();
        }
    });
}

int IceTransport::Outgoing(CopyOnWriteBuffer out_packet, const PacketOptions& options) {
    int ret = -1;
#if !USE_NICE
    // Explicit Congestion Notification takes the least-significant 2 bits of the DS field.
    int ds = int(uint8_t(options.dscp) << 2);
    ret = juice_send_diffserv(juice_agent_.get(), out_packet.data(), out_packet.size(), ds);
#else
    if (last_dscp_ != options.dscp) {
        last_dscp_ = options.dscp;
        // Explicit Congestion Notification takes the least-significant 2 bits of the DS field
        int ds = int(uint8_t(last_dscp_) << 2);
        // ToS is the lagacy name for DS
        nice_agent_set_stream_tos(nice_agent_.get(), stream_id_, ds);
    }
    ret = nice_agent_send(nice_agent_.get(), stream_id_, component_id_, out_packet.size(), reinterpret_cast<const char*>(out_packet.data()));
#endif
    PLOG_VERBOSE << "Send size=" << ret;
    return ret;
}

void IceTransport::Incoming(CopyOnWriteBuffer in_packet) {
    ForwardIncomingPacket(std::move(in_packet));
}

} // namespace naivertc