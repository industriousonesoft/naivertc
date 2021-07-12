#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <signaling/ayame/ayame_channel.hpp>
#include <pc/peer_connection.hpp>

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

private:
    void CreatePeerConnection(const naivertc::RtcConfiguration& rtc_config);

private:
    void OnConnected(bool is_initiator) override;
    void OnClosed(boost::system::error_code ec) override;
    void OnIceServers(std::vector<naivertc::IceServer> ice_servers) override;
    void OnRemoteSDP(const std::string sdp, bool is_offer) override;
    void OnRemoteCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) override;

private:
    boost::asio::io_context& ioc_;
    std::unique_ptr<naivertc::signaling::AyameChannel> ayame_channel_;

    std::shared_ptr<naivertc::PeerConnection> peer_conn_;
};

#endif