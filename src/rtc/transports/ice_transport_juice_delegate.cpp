#if !USE_NICE
#include "rtc/transports/ice_transport.hpp"
#include "common/utils_random.hpp"
#include "rtc/base/task_utils/task_queue_impl.hpp"

#include <plog/Log.h>

namespace naivertc {

const int kMaxTurnServersCount = 2;

void IceTransport::InitJuice(const Configuration& config) {
    RTC_RUN_ON(&sequence_checker_);
    PLOG_VERBOSE << "Initializing ICE transport (libjuice)";

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

    // Pick a stun server
    for (auto& server : config.ice_servers) {
        if (!server.hostname().empty() && server.type() == IceServer::Type::STUN) {
            juice_config.stun_server_host = server.hostname().c_str();
            juice_config.stun_server_port = server.port() != 0 ? server.port() : 3478 /* STUN UDP Port */;
            break;
        }
    }

    // turn servers
    juice_turn_server_t turn_servers[kMaxTurnServersCount];
    std::memset(turn_servers, 0, sizeof(turn_servers));

    int index = 0;
    for (auto& server : config.ice_servers) {
        if (!server.hostname().empty() && server.type() == IceServer::Type::TURN) {
            turn_servers[index].host = server.hostname().c_str();
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
    if (config.port_range_begin > 1024 || (config.port_range_end != 0 && config.port_range_end != 65535)) {
        juice_config.local_port_range_begin = config.port_range_begin;
        juice_config.local_port_range_end = config.port_range_end;
    }

    // Create agent
    juice_agent_ = decltype(juice_agent_)(juice_create(&juice_config), juice_destroy);

}

void IceTransport::OnJuiceState(juice_state_t state) {
    attached_queue_->Post([this, state](){
        switch (state) {
        case JUICE_STATE_DISCONNECTED:
            UpdateState(State::DISCONNECTED);
            break;
        case JUICE_STATE_CONNECTING:
            UpdateState(State::CONNECTING);
            break;
        case JUICE_STATE_CONNECTED:
            UpdateState(State::CONNECTED);
            break;
        case JUICE_STATE_FAILED:
            UpdateState(State::FAILED);
            break;
        case JUICE_STATE_GATHERING:
            // Gathering is not considerd as a connection state
            break;
        case JUICE_STATE_COMPLETED:
            UpdateState(State::COMPLETED);
            break;
        }
    });
}

void IceTransport::OnJuiceGatheringState(GatheringState state) {
    attached_queue_->Post([this, state](){
        UpdateGatheringState(state);
    });
}

void IceTransport::OnJuiceGatheredCandidate(sdp::Candidate candidate) {
    attached_queue_->Post([this, candidate=std::move(candidate)](){
        OnGatheredCandidate(std::move(candidate));
    });
}

void IceTransport::OnJuiceReceivedData(CopyOnWriteBuffer data) {
    attached_queue_->Post([this, data=std::move(data)](){
        Incoming(std::move(data));
    });
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
        ice_transport->OnJuiceState(state);
    } catch (const std::exception &e) {
        PLOG_WARNING << e.what();
    }
}

void IceTransport::OnJuiceCandidateGathered(juice_agent_t* agent, const char* sdp, void* user_ptr) {
    auto ice_transport = static_cast<IceTransport *>(user_ptr);
    ice_transport->OnJuiceGatheredCandidate(sdp::Candidate(sdp, ice_transport->curr_mid_));
}

void IceTransport::OnJuiceGetheringDone(juice_agent_t* agent, void* user_ptr) {
    auto ice_transport = static_cast<IceTransport *>(user_ptr);
    ice_transport->OnJuiceGatheringState(GatheringState::COMPLETED);
}

void IceTransport::OnJuiceDataReceived(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
    auto ice_transport = static_cast<IceTransport *>(user_ptr);
    auto bytes = reinterpret_cast<const uint8_t*>(data);
    ice_transport->OnJuiceReceivedData(CopyOnWriteBuffer(bytes, size));
}

}

#endif // USE_NICE = 0