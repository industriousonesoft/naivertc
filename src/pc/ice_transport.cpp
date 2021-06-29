#include "pc/ice_transport.hpp"
#include "common/utils.hpp"

#include <plog/Log.h>

#include <memory>

namespace naivertc {

const int kMaxTurnServersCount = 2;

IceTransport::IceTransport(const Configuration& config) 
    : Transport(nullptr), 
    juice_agent_(nullptr, nullptr), 
    curr_mid_("0"),
    role_(sdp::Role::ACT_PASS) {
    
    if (config.enable_ice_tcp) {
        PLOG_WARNING << "ICE-TCP is not supported with libjuice.";
    }

    Initialize(config);
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

// Private Methods
void IceTransport::Initialize(const Configuration& config) {

    juice_log_level_t level;
    auto logger = plog::get();

    switch (logger ? logger->getMaxSeverity() : plog::none) {
    case plog::none:
        level = JUICE_LOG_LEVEL_NONE;
        break;
    case plog::error:
        level = JUICE_LOG_LEVEL_ERROR;
        break;
    case plog::warning:
        level = JUICE_LOG_LEVEL_WARN;
        break;
    case plog::debug:
    case plog::info:
        level = JUICE_LOG_LEVEL_INFO;
        break;
    case plog::fatal:
    case plog::verbose:
        level = JUICE_LOG_LEVEL_VERBOSE;
        break;
    default:
        level = JUICE_LOG_LEVEL_NONE;
        break;
    }

    juice_set_log_handler(IceTransport::OnJuiceLog);
    juice_set_log_level(level);

    juice_config_t juice_config = {};
    juice_config.cb_state_changed = IceTransport::OnJuiceStateChanged;
    juice_config.cb_candidate = IceTransport::OnJuiceCandidateGathered;
    juice_config.cb_gathering_done = IceTransport::OnJuiceGetheringDone;
    juice_config.cb_recv = IceTransport::OnJuiceDataReceived;
    juice_config.user_ptr = this;

    // Randomize ice servers order
    auto ice_servers = config.ice_servers;
    utils::random::shuffle(ice_servers);

    // Pick a stun server
    for (auto& server : ice_servers) {
        if (!server.host_name().empty() && server.type() == IceServer::Type::STUN) {
            juice_config.stun_server_host = server.host_name().c_str();
            juice_config.stun_server_port = server.port() != 0 ? server.port() : 3478 /* STUN UDP Port */;
            break;
        }
    }

    // turn servers
    juice_turn_server_t turn_servers[kMaxTurnServersCount];
    std::memset(turn_servers, 0, sizeof(turn_servers));

    int index = 0;
    for (auto& server : ice_servers) {
        if (!server.host_name().empty() && server.type() == IceServer::Type::TURN) {
            turn_servers[index].host = server.host_name().c_str();
            turn_servers[index].username = server.username().c_str();
            turn_servers[index].password = server.password().c_str();
            turn_servers[index].port = server.port() != 0 ? server.port() : 3478 /* STUN UDP Port */;
            if (++index >= kMaxTurnServersCount) {
                break;
            }
        }
    }

    juice_config.turn_servers = index > 0 ? turn_servers : nullptr;
    juice_config.turn_servers_count = index;

    // Bind address
    if (config.bind_addresses) {
        juice_config.bind_address = config.bind_addresses->c_str();
    }

    // Port range
    if (config.port_range_begin_ > 1024 || (config.port_range_end_ != 0 && config.port_range_end_ != 65535)) {
        juice_config.local_port_range_begin = config.port_range_begin_;
        juice_config.local_port_range_end = config.port_range_end_;
    }

    // Create agent
    juice_agent_ = decltype(juice_agent_)(juice_create(&juice_config), juice_destroy);

}

void IceTransport::OnJuiceStateChanged(State state) {
    Transport::SignalStateChanged(state);
}

void IceTransport::OnJuiceCandidateGathered(const char* sdp) {
    SignalCandidateGathered(std::move(Candidate(sdp, curr_mid_)));
}

void IceTransport::OnJuiceGetheringStateChanged(GatheringState state) {
    SignalGatheringStateChanged(state);
}

void IceTransport::OnJuiceDataReceived(const char* data, size_t size) {
    try {
        PLOG_VERBOSE << "Incoming size: " << size;
        // 使用reinterpret_cast(re+interpret+cast：重新诠释转型)对data中的数据格式进行重新映射: char -> byte
        // auto b = reinterpret_cast<const std::byte *>(data);
    } catch(const std::exception &e) {
        PLOG_WARNING << e.what();
    }
}

// Juice callback methods
void IceTransport::OnJuiceLog(juice_log_level_t level, const char* message) {
    plog::Severity severity;
	switch (level) {
	case JUICE_LOG_LEVEL_FATAL:
		severity = plog::fatal;
		break;
	case JUICE_LOG_LEVEL_ERROR:
		severity = plog::error;
		break;
	case JUICE_LOG_LEVEL_WARN:
		severity = plog::warning;
		break;
	case JUICE_LOG_LEVEL_INFO:
		severity = plog::info;
		break;
	default:
		severity = plog::verbose; // libjuice debug as verbose
		break;
	}
	PLOG(severity) << "juice: " << message;
}

void IceTransport::OnJuiceStateChanged(juice_agent_t* agent, juice_state_t state, void* user_ptr) {
    auto ice_transport = static_cast<IceTransport *>(user_ptr);
    try {
        switch (state) {
        case JUICE_STATE_DISCONNECTED:
            ice_transport->OnJuiceStateChanged(State::DISCONNECTED);
            break;
        case JUICE_STATE_CONNECTING:
            ice_transport->OnJuiceStateChanged(State::CONNECTING);
            break;
        case JUICE_STATE_CONNECTED:
            ice_transport->OnJuiceStateChanged(State::CONNECTED);
            break;
        case JUICE_STATE_FAILED:
            ice_transport->OnJuiceStateChanged(State::FAILED);
            break;
        case JUICE_STATE_GATHERING:
            // Gathering is not considerd as a connection state
            ice_transport->OnJuiceGetheringStateChanged(GatheringState::GATHERING);
            break;
        case JUICE_STATE_COMPLETED:
            ice_transport->OnJuiceStateChanged(State::COMPLETED);
            break;
        }
    } catch (const std::exception &e) {
        PLOG_WARNING << e.what();
    }
}

void IceTransport::OnJuiceCandidateGathered(juice_agent_t* agent, const char* sdp, void* user_ptr) {
    auto ice_transport = static_cast<IceTransport *>(user_ptr);
    ice_transport->OnJuiceCandidateGathered(sdp);
}

void IceTransport::OnJuiceGetheringDone(juice_agent_t* agent, void* user_ptr) {
    auto ice_transport = static_cast<IceTransport *>(user_ptr);
    ice_transport->OnJuiceGetheringStateChanged(GatheringState::COMPLETED);
}

void IceTransport::OnJuiceDataReceived(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
    auto ice_transport = static_cast<IceTransport *>(user_ptr);
    ice_transport->OnJuiceDataReceived(data, size);
}

}