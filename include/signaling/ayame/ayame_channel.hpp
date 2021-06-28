#ifndef _AYAME_CHANNEL_H_
#define _AYAME_CHANNEL_H_

#include "signaling/websocket.hpp"
#include "signaling/base_channel.hpp"

// nlohmann/json
#include <nlohmann/json.hpp>

#include <memory>
#include <string>

namespace naivertc {
namespace signaling {

class AyameChannel : public BaseChannel {
public:
    AyameChannel(boost::asio::io_context& ioc, std::weak_ptr<Observer> observer);
    ~AyameChannel() override;
    
public:
    void Connect(Config config) override;
    void Close() override;
    void SendLocalSDP(const std::string sdp, bool is_offer) override;
    void SendLocalCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string sdp) override;

private:
    bool ParseURL(const std::string signaling_url, URLParts& parts);

private:
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
    Config config_;
    std::unique_ptr<Websocket> ws_;
    
    std::atomic<bool> is_connected_;
    std::atomic<bool> is_connecting_;
    std::atomic<bool> is_closing_;
    
    std::vector<naivertc::IceServer> ice_servers_;
};

}
}
#endif
