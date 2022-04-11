/*
 * @Description: 
 * @Version: 
 * @Author: CaoWanPing
 * @Date: 2021-03-03 11:49:47
 * @LastEditTime: 2021-03-25 15:44:32
 */

#include "channels/ayame_channel.hpp"

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

AyameChannel::AyameChannel(boost::asio::io_context& ioc, Observer* observer)
    : BaseChannel(ioc, observer) {}

AyameChannel::~AyameChannel() = default;

void AyameChannel::DoRegister() {
    json json_message = {
        {"type", "register"},
        {"clientId", config_.client_id},
        {"roomId", config_.room_id},
        {"AyameChannel", "WebRTC Native Client"},
        {"libwebrtc", "m86.0.4240.198"},
        {"environment", "Cross Platform"},
    };
    if (config_.signaling_key.length() > 0) {
        json_message["key"] = config_.signaling_key;
    }
    ws_->WriteText(json_message.dump());
}

bool AyameChannel::OnIncomingMessage(std::string text) {
   
    auto json_message = json::parse(text);
    const std::string type = json_message["type"];
    if (type == "accept") {
        auto is_initiator = false;
        if (json_message.contains("isInitiator")) {
            is_initiator = json_message["isInitiator"];
        }
        
        ParseIceServers(json_message, ice_servers_);
        observer_->OnIceServers(ice_servers_);
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
        Close();
        return false;
    } else if (type == "error") {
        Close();
        return false;
    }
    return true;
}

void AyameChannel::DoSendPong() {
    json json_message = {{"type", "pong"}};
    ws_->WriteText(json_message.dump());
}

} // namespace signaling
