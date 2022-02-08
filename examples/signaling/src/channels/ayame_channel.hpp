#ifndef _AYAME_CHANNEL_H_
#define _AYAME_CHANNEL_H_

#include "base/websocket.hpp"
#include <rtc/pc/peer_connection_configuration.hpp>

// nlohmann/json
#include <nlohmann/json.hpp>

#include <memory>
#include <string>

namespace naivertc {
namespace signaling {

class AyameChannel {
public:
    // Configuration
    struct Configuration {
        bool insecure = false;
        
        std::string signaling_url;
        std::string room_id;
        std::string client_id;
        std::string signaling_key;
        
        std::vector<std::string> ice_server_urls;
    };

    // Observer
    class Observer {
    public:
        virtual ~Observer() = default;
        virtual void OnConnected(bool is_initiator) = 0;
        virtual void OnClosed(boost::system::error_code ec) = 0;
        virtual void OnIceServers(std::vector<naivertc::IceServer> ice_servers) = 0;
        virtual void OnRemoteSDP(const std::string sdp, bool is_offer) = 0;
        virtual void OnRemoteCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) = 0;
    };
public:
    AyameChannel(boost::asio::io_context& ioc, Observer* observer);
    ~AyameChannel();
    
    void Connect(Configuration config);
    void Close();
    void SendLocalSDP(const std::string sdp, bool is_offer);
    void SendLocalCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string sdp);

private:
    bool ParseURL(const std::string& signaling_url, URLParts& parts);

    void DoRead();
    void DoFetchAyameIceServer();
    void DoRegister();
    void DoSendPong();
    void ParseIceServers(nlohmann::json json_message, std::vector<naivertc::IceServer>& ice_servers);
    void CreatePeerConnection();

 private:
    void OnConnect(boost::system::error_code ec);
    void OnClose(boost::system::error_code ec);
    void OnRead(boost::system::error_code ec,
              std::size_t bytes_transferred,
              std::string text);

private:
    boost::asio::io_context& ioc_;
    Configuration config_;
    std::unique_ptr<Websocket> ws_;
    Observer* const observer_;
    
    std::atomic<bool> is_connected_;
    std::atomic<bool> is_connecting_;
    std::atomic<bool> is_closing_;
    
    std::vector<naivertc::IceServer> ice_servers_;
};

}
}
#endif
