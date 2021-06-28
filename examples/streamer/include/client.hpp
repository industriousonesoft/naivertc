#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <signaling/base_channel.hpp>
#include <signaling/ayame/ayame_channel.hpp>

class Client: public naivertc::signaling::BaseChannel::Observer, public std::enable_shared_from_this<Client> {
    Client(boost::asio::io_context& ioc);
public:
    static std::shared_ptr<Client> Create(boost::asio::io_context& ioc) {
        return std::shared_ptr<Client>(new Client(ioc));
    }
    // shared_ptr在引用为0是需要销毁对象，因此需确保析构函数是公开的
    virtual ~Client();

    void Start();
    void Stop();
public:
    void OnConnected(bool is_initiator) override;
    void OnClosed(boost::system::error_code ec) override;
    void OnIceServers(std::vector<naivertc::IceServer> ice_servers) override;
    void OnRemoteSDP(const std::string sdp, bool is_offer) override;
    void OnRemoteCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) override;

private:
    std::unique_ptr<naivertc::signaling::AyameChannel> ayame_channel_;
};

#endif