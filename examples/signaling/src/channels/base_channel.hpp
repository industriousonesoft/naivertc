#ifndef _SIGNALING_CHANNELS_CHANNEL_H_
#define _SIGNALING_CHANNELS_CHANNEL_H_

#include <rtc/pc/ice_server.hpp>
#include "components/websocket.hpp"

#include <memory>
#include <string>

namespace signaling {

class BaseChannel {
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
    BaseChannel(boost::asio::io_context& ioc, Observer* observer);
    virtual ~BaseChannel();
    
    void Connect(Configuration config);
    void Close();
    void SendSDP(std::string_view sdp, bool is_offer);
    void SendCandidate(std::string_view sdp_mid, 
                       int sdp_mlineindex, 
                       std::string_view sdp);

protected:
    virtual void DoRegister();
    // Return true for more read.
    virtual bool OnIncomingMessage(std::string text);

private:
    void OnConnect(boost::system::error_code ec);
    void OnClose(boost::system::error_code ec);
    void OnRead(boost::system::error_code ec,
              std::size_t bytes_transferred,
              std::string text);

    void DoRead();

protected:
    boost::asio::io_context& ioc_;
    Configuration config_;
    std::unique_ptr<Websocket> ws_;
    Observer* const observer_;
    
    std::atomic<bool> is_connected_;
    std::atomic<bool> is_connecting_;
    std::atomic<bool> is_closing_;
    
    std::vector<naivertc::IceServer> ice_servers_;
};

} // namespace signaling


#endif