#ifndef _SIGNALING_CHANNEL_H_
#define _SIGNALING_CHANNEL_H_

#include "pc/configuration.hpp"

#include <boost/system/error_code.hpp>

#include <string>
#include <vector>

namespace naivertc {
namespace signaling {
class BaseChannel {
public:
    class Observer {
    public:
        virtual void OnConnected(bool is_initiator) = 0;
        virtual void OnClosed(boost::system::error_code ec) = 0;
        virtual void OnIceServers(std::vector<naivertc::IceServer> ice_servers) = 0;
        virtual void OnRemoteSDP(const std::string sdp, bool is_offer) = 0;
        virtual void OnRemoteCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) = 0;
    };
public:
    struct Config {
        bool insecure = false;
        
        std::string signaling_url;
        std::string room_id;
        std::string client_id;
        std::string signaling_key;
        
        std::vector<std::string> ice_server_urls;
    };
public:
    BaseChannel(Observer* observer);
    virtual ~BaseChannel() = default;
    virtual void Connect(Config config) = 0;
    virtual void Close() = 0;
    virtual void SendLocalSDP(const std::string sdp, bool is_offer) = 0;
    virtual void SendLocalCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) = 0;

protected:
    Observer* observer_;
};

}
}

#endif
