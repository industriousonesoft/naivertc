#if USE_NICE
#include "rtc/transports/ice_transport.hpp"
#include "common/utils.hpp"

#include <plog/Log.h>

namespace naivertc {

void IceTransport::InitNice(const RtcConfiguration& config) {
    PLOG_VERBOSE << "Initializing ICE transport (libnice)";

    g_log_set_handler("libnice", G_LOG_LEVEL_MASK, OnNiceLog, this);

    IF_PLOG(plog::verbose) {
        // do not output STUN debug message
        nice_debug_enable(false);
    }

    main_loop_ = decltype(main_loop_)(g_main_loop_new(nullptr /* context */, false /* is_running */), g_main_loop_unref);
    if (!main_loop_) {
        throw std::runtime_error("Failed to create the nice main loop");
    }

    // RFC 5245 was obsoleted by RFC 8445 but this should be OK.
    // See https://datatracker.ietf.org/doc/html/rfc5245
    nice_agent_ = decltype(nice_agent_)(nice_agent_new(g_main_loop_get_context(main_loop_.get()), NICE_COMPATIBILITY_RFC5245), g_object_unref);
    if (!nice_agent_) {
        throw std::runtime_error("Failed to create the nice agent");
    }

    main_loop_thread_ = std::thread(g_main_loop_run, main_loop_.get());

    stream_id_ = nice_agent_add_stream(nice_agent_.get(), component_id_);
    if (!stream_id_) {
        throw std::runtime_error("Failed to add a nice stream");
    }

    g_object_set(G_OBJECT(nice_agent_.get()), "controlling-mode", TRUE, nullptr);
    g_object_set(G_OBJECT(nice_agent_.get()), "ice-udp", TRUE, nullptr);
    g_object_set(G_OBJECT(nice_agent_.get()), "ice-tcp", config.enable_ice_tcp ? TRUE : FALSE, nullptr);

    // RFC 8445: Agents MUST NOT use an RTO value smaller than 500 ms.
    g_object_set(G_OBJECT(nice_agent_.get()), "stun-initial-timeout", 500, nullptr);
    g_object_set(G_OBJECT(nice_agent_.get()), "stun-max-retransmissions", 3, nullptr);

    // RFC 8445: ICE agents SHOULD use a default Ta value, 50ms, but MAY use another value based on 
    // the characteristics of the associated data. 
    g_object_set(G_OBJECT(nice_agent_.get()), "stun-pacing-timer", 25, nullptr);

	g_object_set(G_OBJECT(nice_agent_.get()), "upnp", FALSE, nullptr);
	g_object_set(G_OBJECT(nice_agent_.get()), "upnp-timeout", 200, nullptr);

    // Proxy
    if (config.proxy_server.has_value()) {
        auto proxy_server = config.proxy_server.value();
        g_object_set(G_OBJECT(nice_agent_.get()), "proxy-type", proxy_server.type, nullptr);
		g_object_set(G_OBJECT(nice_agent_.get()), "proxy-ip", proxy_server.hostname.c_str(), nullptr);
		g_object_set(G_OBJECT(nice_agent_.get()), "proxy-port", proxy_server.port, nullptr);
		g_object_set(G_OBJECT(nice_agent_.get()), "proxy-username", proxy_server.username.c_str(), nullptr);
		g_object_set(G_OBJECT(nice_agent_.get()), "proxy-password", proxy_server.password.c_str(), nullptr);
    }

    // Randomize order
    auto ice_servers = config.ice_servers;
    utils::random::shuffle(ice_servers);

    // Pick one STUN server
    for (auto &ice_server : ice_servers) {
        if (ice_server.hostname().empty()) {
            continue;
        }
        if (ice_server.type() != IceServer::Type::STUN) {
            continue;
        }
        uint16_t server_port = ice_server.port();
        if (server_port == 0) {
            // STUN UDP port
            server_port = 3478;
        }

        auto resovle_result = utils::network::IPv4Resolve(ice_server.hostname(), std::to_string(server_port), utils::network::ProtocolType::UDP, false);

        if (resovle_result.has_value()) {
            PLOG_INFO << "Using STUN server: " << ice_server.hostname() << ":" << server_port;
            g_object_set(G_OBJECT(nice_agent_.get()), "stun-server", resovle_result.value().address.c_str(), nullptr);
			g_object_set(G_OBJECT(nice_agent_.get()), "stun-server-port", resovle_result.value().port, nullptr);
            break;
        }
    }

    // Add TURN servers
    for (auto &ice_server : ice_servers) {
        if (ice_server.hostname().empty()) {
            continue;
        }
        if (ice_server.type() != IceServer::Type::TURN) {
            continue;
        }
        uint16_t server_port = ice_server.port();
        if (server_port == 0) {
            // STUN UDP port
            server_port = ice_server.relay_type() == IceServer::RelayType::TURN_TLS ? 5349 : 3478;
        }

        auto protocol_type = ice_server.relay_type() == IceServer::RelayType::TURN_UDP ? utils::network::ProtocolType::UDP : utils::network::ProtocolType::TCP;
        auto resolve_result = utils::network::UnspecfiedResolve(ice_server.hostname(), std::to_string(server_port), protocol_type, false);

        if (resolve_result.has_value()) {
            PLOG_INFO << "Using TURN server: " << ice_server.hostname() << ":" << server_port;
            NiceRelayType nice_relay_type = NICE_RELAY_TYPE_TURN_UDP;
            switch (ice_server.relay_type())
            {
            case IceServer::RelayType::TURN_TLS:
                nice_relay_type = NICE_RELAY_TYPE_TURN_TCP;
                break;
            case IceServer::RelayType::TURN_TCP:
                nice_relay_type = NICE_RELAY_TYPE_TURN_TLS;
                break;
            default:
                nice_relay_type = NICE_RELAY_TYPE_TURN_UDP;
                break;
            }

            nice_agent_set_relay_info(nice_agent_.get(), stream_id_, component_id_, resolve_result.value().address.c_str(), resolve_result.value().port, ice_server.username().c_str(), ice_server.password().c_str(), nice_relay_type);
        }
    }

    g_signal_connect(G_OBJECT(nice_agent_.get()), "component-state-changed", G_CALLBACK(OnNiceStateChanged), this);
    g_signal_connect(G_OBJECT(nice_agent_.get()), "new-candidate-full", G_CALLBACK(OnNiceCandidateGathered), this);
    g_signal_connect(G_OBJECT(nice_agent_.get()), "candidate-gathering-done", G_CALLBACK(OnNiceGetheringDone), this);

    nice_agent_set_stream_name(nice_agent_.get(), stream_id_, "application");
    nice_agent_set_port_range(nice_agent_.get(), stream_id_, component_id_, config.port_range_begin, config.port_range_end);

    nice_agent_attach_recv(nice_agent_.get(), stream_id_, component_id_, g_main_loop_get_context(main_loop_.get()), OnNiceDataReceived, this);

}

std::string IceTransport::NiceAddressToString(const NiceAddress& nice_addr) const {
    char buffer[NICE_ADDRESS_STRING_LEN];
    nice_address_to_string(&nice_addr, buffer);
    unsigned int port = nice_address_get_port(&nice_addr);
    return std::string(buffer) + ":" + std::to_string(port);
}

void IceTransport::ProcessNiceTimeout() {
    task_queue_.Post([this](){
        PLOG_WARNING << "ICE timeout";
        timeout_id_ = 0;
        OnStateChanged(State::FAILED);
    });
}

void IceTransport::ProcessNiceState(guint state) {
    if (state == NICE_COMPONENT_STATE_FAILED && trickle_timeout_.count() > 0) {
        if (timeout_id_)
            g_source_remove(timeout_id_);
        timeout_id_ = g_timeout_add(trickle_timeout_.count() /* ms */, OnNiceTimeout, this);
        return;
    }

    if (state == NICE_COMPONENT_STATE_CONNECTED && timeout_id_) {
        g_source_remove(timeout_id_);
        timeout_id_ = 0;
    }

    switch (state) {
    case NICE_COMPONENT_STATE_DISCONNECTED:
        OnStateChanged(State::DISCONNECTED);
        break;
    case NICE_COMPONENT_STATE_CONNECTING:
        OnStateChanged(State::CONNECTING);
        break;
    case NICE_COMPONENT_STATE_CONNECTED:
        OnStateChanged(State::CONNECTED);
        break;
    case NICE_COMPONENT_STATE_READY:
        OnStateChanged(State::COMPLETED);
        break;
    case NICE_COMPONENT_STATE_FAILED:
        OnStateChanged(State::FAILED);
        break;
    };
}

// libnice callbacks
void IceTransport::OnNiceLog(const gchar* log_domain, GLogLevelFlags log_level, const gchar* message, gpointer user_data) {
    plog::Severity severity;
	unsigned int flags = log_level & G_LOG_LEVEL_MASK;
	if (flags & G_LOG_LEVEL_ERROR)
		severity = plog::fatal;
	else if (flags & G_LOG_LEVEL_CRITICAL)
		severity = plog::error;
	else if (flags & G_LOG_LEVEL_WARNING)
		severity = plog::warning;
	else if (flags & G_LOG_LEVEL_MESSAGE)
		severity = plog::info;
	else if (flags & G_LOG_LEVEL_INFO)
		severity = plog::info;
	else
		severity = plog::verbose; // libnice debug as verbose

	PLOG(severity) << "nice: " << message;
}

void IceTransport::OnNiceStateChanged(NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer user_data) {
    auto ice_transport = static_cast<IceTransport*>(user_data);
    try {
        ice_transport->ProcessNiceState(state);
    }catch (const std::exception& exp) {
        PLOG_WARNING << exp.what();
    }
}

void IceTransport::OnNiceCandidateGathered(NiceAgent *agent, NiceCandidate *candidate, gpointer user_data) {
    auto ice_transport = static_cast<IceTransport*>(user_data);
    try {
        gchar* sdp = nice_agent_generate_local_candidate_sdp(agent, candidate);
        ice_transport->OnCandidateGathered(std::move(sdp));
    }catch (const std::exception& exp) {
        PLOG_WARNING << exp.what();
    }
}

void IceTransport::OnNiceGetheringDone(NiceAgent *agent, guint stream_id, gpointer user_data) {
    auto ice_transport = static_cast<IceTransport*>(user_data);
    ice_transport->OnGetheringStateChanged(GatheringState::COMPLETED);
}

void IceTransport::OnNiceDataReceived(NiceAgent *agent, guint stream_id, guint component_id, guint size, gchar *data, gpointer user_data) {
    auto ice_transport = static_cast<IceTransport*>(user_data);
    try {
        ice_transport->OnDataReceived(std::move(data), size);
    }catch (const std::exception& exp) {
        PLOG_WARNING << exp.what();
    }
}

gboolean IceTransport::OnNiceTimeout(gpointer user_data) {
    auto ice_transport = static_cast<IceTransport*>(user_data);
    try {
        ice_transport->ProcessNiceTimeout();
    }catch (const std::exception& exp) {
        PLOG_WARNING << exp.what();
    }
    return false;
}
    
} // namespace naivertc


#endif // USE_NICE = 1