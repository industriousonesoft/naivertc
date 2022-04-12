#ifndef _AYAME_CHANNEL_H_
#define _AYAME_CHANNEL_H_

#include "channels/signaling_channel.hpp"

#include <string>
#include <rtc/pc/ice_server.hpp>

namespace signaling {

class Client : public Channel::Observer {
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
        virtual void OnClosed(const std::string err_reason) = 0;
        virtual void OnIceServers(std::vector<naivertc::IceServer> ice_servers) = 0;
        virtual void OnRemoteSDP(const std::string sdp, bool is_offer) = 0;
        virtual void OnRemoteCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) = 0;
    };
public:
    Client(const Configuration& config, 
                boost::asio::io_context& ioc, 
                Observer* observer);
    ~Client() override;

    void Start();
    void Stop();

    void SendSDP(std::string_view sdp, bool is_offer);
    void SendCandidate(std::string_view sdp_mid, 
                       int sdp_mlineindex, 
                       std::string_view sdp);

private:
    // Implements of Channel::Observer
    void OnConnected() override;
    void OnClosed(const std::string err_reason) override;
    bool OnRead(const std::string msg) override;
    
private:
    void DoRegister();
    void DoSendPong();

private:
    const Configuration config_;
    std::unique_ptr<Channel> channel_;
    Observer* const observer_;

};

} // namespace signaling

#endif
