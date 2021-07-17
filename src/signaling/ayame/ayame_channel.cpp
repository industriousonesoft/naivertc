/*
 * @Description: 
 * @Version: 
 * @Author: CaoWanPing
 * @Date: 2021-03-03 11:49:47
 * @LastEditTime: 2021-03-25 15:44:32
 */

#include <iostream>

#include "signaling/ayame/ayame_channel.hpp"

// plog
#include <plog/Log.h>

namespace naivertc {
namespace signaling {

using json = nlohmann::json;

bool AyameChannel::ParseURL(const std::string& signaling_url, URLParts& parts) {
  if (!URLParts::Parse(signaling_url, parts)) {
    throw std::exception();
  }

  std::string default_port;
  if (parts.scheme == "wss") {
    return true;
  } else if (parts.scheme == "ws") {
    return false;
  } else {
    throw std::exception();
  }
}

AyameChannel::AyameChannel(boost::asio::io_context& ioc, std::weak_ptr<Observer> observer)
                        : BaseChannel(observer),
                          ioc_(ioc),
                          is_connected_(false) {
}

AyameChannel::~AyameChannel() {
    Close();
    if (ws_) {
        ws_.reset();
    }
    ice_servers_.clear();
}

void AyameChannel::Connect(Config config) {
    if (is_connected_ || is_connecting_) {
        return;
    }
    is_connecting_ = true;
    config_ = config;
    std::string signaling_url = config_.signaling_url;
    URLParts parts;
    
    if (config.ice_server_urls.size() > 0) {
        for (const std::string& url : config.ice_server_urls) {
            naivertc::IceServer ice_server(url);
            ice_servers_.push_back(ice_server);
        }
    }

    // FIXME: error occuring when signaling_url == ""
    if (ParseURL(signaling_url, parts)) {
        ws_.reset(new Websocket(ioc_, Websocket::ssl_tag(), config_.insecure));
    } else {
        ws_.reset(new Websocket(ioc_));
    }
    ws_->Connect(signaling_url,
               std::bind(&AyameChannel::OnConnect, this,
                         std::placeholders::_1));
}

void AyameChannel::OnConnect(boost::system::error_code ec) {
    is_connecting_ = false;
    if (ec) {
        is_connected_ = false;
        if (auto ob = observer_.lock()) {
            ob->OnClosed(ec);
        }
        return;
    }
    is_connected_ = true;
    DoRead();
    DoRegister();
}

void AyameChannel::DoRead() {
    ws_->Read(std::bind(&AyameChannel::OnRead, this,
                        std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3));
}

void AyameChannel::DoFetchAyameIceServer() {
    json json_message = {
        {"type", "register"},
        {"clientId", "AyameFetcher"},
        {"roomId", "industriousonesoft@ayame-labo-sample"},
        {"AyameChannel", "WebRTC Native Client"},
        {"libwebrtc", "m86.0.4240.198"},
        {"environment", "Cross Platform"},
        {"key", "dzSU5Lz88dfZ0mVTWp51X8bPKBzfmhfdZH8D2ei3U7aNplX6"}
    };
    ws_->WriteText(json_message.dump());
}

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

void AyameChannel::DoSendPong() {
    json json_message = {{"type", "pong"}};
    ws_->WriteText(json_message.dump());
}

void AyameChannel::ParseIceServers(json json_message, std::vector<naivertc::IceServer>& ice_servers) {
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
    // If iceServers are not returned at the time of accept, use google's stun server
    if (ice_servers.empty()) {
        naivertc::IceServer ice_server("stun:stun.l.google.com:19302");
        ice_servers.push_back(ice_server);
    }
}

void AyameChannel::Close() {
    if (!is_connected_ || is_closing_) {
        return;
    }
    is_closing_ = true;
    ws_->Close(std::bind(&AyameChannel::OnClose, this,
                       std::placeholders::_1));
}

void AyameChannel::OnClose(boost::system::error_code ec) {
    if (ec) {
        std::cout << __FUNCTION__ << ec.message() << std::endl;
    }
    is_closing_ = false;
    is_connected_ = false;
    if (auto ob = observer_.lock()) {
        ob->OnClosed(ec);
    }
}

void AyameChannel::OnRead(boost::system::error_code ec,
                         std::size_t bytes_transferred,
                         std::string text) {
   
    boost::ignore_unused(bytes_transferred);

    // Do not treat this as an error as this error will occur when the read process is canceled due to writing
    if (ec == boost::asio::error::operation_aborted)
        return;

    // If WebSocket returns a closed error, immediately call Close (); to exit the OnRead function.
    if (ec == boost::beast::websocket::error::closed) {
        // When WebSocket is closed by Close () ;, the function is called in the order of OnClose ();-> ReconnectAfter ();-> OnWatchdogExpired () ;.
        // WebSocket is reconnected
        Close();
        return;
    }

    if (ec) {
        std::cout << __FUNCTION__ << ": error: " << ec.message() << std::endl;
        if (auto ob = observer_.lock()) {
            ob->OnClosed(ec);
        }
        return;
    }

//    std::cout << __FUNCTION__ << ": text=" << text << std::endl;

    auto json_message = json::parse(text);
    const std::string type = json_message["type"];
    if (type == "accept") {
        auto is_initiator = false;
        if (json_message.contains("isInitiator")) {
            is_initiator = json_message["isInitiator"];
        }
        
        ParseIceServers(json_message, ice_servers_);
        if (auto ob = observer_.lock()) {
            ob->OnIceServers(ice_servers_);
            ob->OnConnected(is_initiator);
        }
        
    } else if (type == "offer") {
        const std::string sdp = json_message["sdp"];
        if (auto ob = observer_.lock()) {
            ob->OnRemoteSDP(sdp, true);
        }
    
    } else if (type == "answer") {
        const std::string sdp = json_message["sdp"];
        if (auto ob = observer_.lock()) {
            ob->OnRemoteSDP(sdp, false);
        }
    } else if (type == "candidate") {
        int sdp_mlineindex = 0;
        std::string sdp_mid, candidate;
        json ice = json_message["ice"];
        sdp_mid = ice["sdpMid"].get<std::string>();
        sdp_mlineindex = ice["sdpMLineIndex"].get<int>();
        candidate = ice["candidate"].get<std::string>();
        if (auto ob = observer_.lock()) {
            ob->OnRemoteCandidate(sdp_mid, sdp_mlineindex, candidate);
        }
    } else if (type == "ping") {
        DoSendPong();
    } else if (type == "bye") {
        std::cout << __FUNCTION__ << ": bye" << std::endl;
        Close();
        return;
    } else if (type == "error") {
        std::cout << __FUNCTION__ << ": error => " << text << std::endl;
        Close();
        return;
    }
    DoRead();
}

void AyameChannel::SendLocalSDP(const std::string sdp, bool is_offer) {
    json json_message = {
        {"type", is_offer ? "offer" : "answer"}, 
        {"sdp", std::move(sdp)}
    };
    ws_->WriteText(json_message.dump());
}

void AyameChannel::SendLocalCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) {
    // ayame uses the `ice` property in exchange for candidate sdp. Note that it is not `candidate`
    json json_message = {
        {"type", "candidate"}
    };
    // Set candidate information as object in ice property and send
    json_message["ice"] = {{"candidate", std::move(candidate)},
                            {"sdpMLineIndex", sdp_mlineindex},
                            {"sdpMid", std::move(sdp_mid)}};
    ws_->WriteText(json_message.dump());
}

}
}
