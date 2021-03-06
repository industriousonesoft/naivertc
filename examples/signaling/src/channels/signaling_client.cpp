#include "channels/signaling_client.hpp"

#include <nlohmann/json.hpp>
#include <plog/Log.h>

namespace signaling {
namespace {

using json = nlohmann::json;

void ParseIceServers(json json_message, std::vector<naivertc::IceServer>& ice_servers) {
    //Ice servers receiced from Ayame
    if (json_message.contains("iceServers")) {
        auto jservers = json_message["iceServers"];
        if (jservers.is_array()) {
            for (auto jserver : jservers) {
                auto jurls = jserver["urls"];
                for (const std::string url : jurls) {
                    naivertc::IceServer ice_server(url);
                    if (ice_server.type() == naivertc::IceServer::Type::TURN && 
                        jserver.contains("username") && 
                        jserver.contains("credential")) {
                        ice_server.set_username(jserver["username"].get<std::string>());
                        ice_server.set_password(jserver["credential"].get<std::string>());
                    }
                    PLOG_VERBOSE << "iceserver = " << std::string(ice_server);
                    ice_servers.push_back(ice_server);
                }
                
            }
        }
    }
    // If no ice servers is available, falling back to google's stun server
    if (ice_servers.empty()) {
        naivertc::IceServer ice_server("stun:stun.l.google.com:19302");
        ice_servers.push_back(ice_server);
    }
}
    
} // namespace

Client::Client(const Configuration& config, 
                         boost::asio::io_context& ioc, 
                         Observer* observer)
    : config_(config),
      channel_(CreateDefaultChannel(ioc, this)),
      observer_(observer) {
    assert(channel_ != nullptr);
    assert(observer_ != nullptr);
}

Client::~Client() {
    Stop();
}

void Client::Start() {
    channel_->Connect(config_.signaling_url, config_.insecure);
}

void Client::Stop() {
    channel_->Close();
}

void Client::SendSDP(std::string_view sdp, bool is_offer) {
    json json_message = {
        {"type", is_offer ? "offer" : "answer"}, 
        {"sdp", sdp}
    };
    channel_->Send(json_message.dump());
}

void Client::SendCandidate(std::string_view sdp_mid, 
                                int sdp_mlineindex, 
                                std::string_view candidate) {
    // ayame uses the `ice` property in exchange for candidate sdp. Note that it is not `candidate`
    json json_message = {
        {"type", "candidate"}
    };
    // Set candidate information as object in ice property and send
    json_message["ice"] = {{"candidate", candidate},
                            {"sdpMLineIndex", sdp_mlineindex},
                            {"sdpMid", sdp_mid}};
    channel_->Send(json_message.dump());
}

// Private methods
void Client::OnConnected() {
    DoRegister();
}

void Client::OnClosed(const std::string err_reason) {
    observer_->OnClosed(err_reason);
}

bool Client::OnRead(const std::string msg) {

    auto json_message = json::parse(msg);
    const std::string type = json_message["type"];
    // Register accepted.
    if (type == "accept") {
        auto is_initiator = false;
        if (json_message.contains("isInitiator")) {
            is_initiator = json_message["isInitiator"];
        }

        std::vector<naivertc::IceServer> ice_servers;
        if (!config_.ice_server_urls.empty()) {
            for (std::string url : config_.ice_server_urls) {
                naivertc::IceServer ice_server(std::move(url));
                ice_servers.push_back(ice_server);
            }
        }
        
        ParseIceServers(json_message, ice_servers);
        observer_->OnIceServers(ice_servers);
        observer_->OnConnected(is_initiator);
        
    } else if (type == "offer") {
        std::string sdp = json_message["sdp"];
        observer_->OnRemoteSDP(std::move(sdp), true);
    
    } else if (type == "answer") {
        std::string sdp = json_message["sdp"];
        observer_->OnRemoteSDP(std::move(sdp), false);
    } else if (type == "candidate") {
        int sdp_mlineindex = 0;
        std::string sdp_mid, candidate;
        json ice = json_message["ice"];
        sdp_mid = ice["sdpMid"].get<std::string>();
        sdp_mlineindex = ice["sdpMLineIndex"].get<int>();
        candidate = ice["candidate"].get<std::string>();
        observer_->OnRemoteCandidate(sdp_mid, sdp_mlineindex, candidate);
    } else if (type == "ping") {
        DoSendPong();
    } else if (type == "bye") {
        Stop();
        observer_->OnClosed("Closed by remote.");
        return false;
    } else if (type == "error") {
        Stop();
        std::string err = json_message["error"];
        observer_->OnClosed(err);
        return false;
    }
    return true;
}

// Private methods
void Client::DoRegister() {
    json json_message = {
        {"type", "register"},
        {"clientId", config_.client_id},
        {"roomId", config_.room_id},
        {"Client", "WebRTC Native Client"},
        {"libwebrtc", "m86.0.4240.198"},
        {"environment", "Cross Platform"}
    };
    if (config_.signaling_key.length() > 0) {
        json_message["key"] = config_.signaling_key;
    }
    channel_->Send(json_message.dump());
}

void Client::DoSendPong() {
    json json_message = {{"type", "pong"}};
    channel_->Send(json_message.dump());
}

} // namespace signaling
