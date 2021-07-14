#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <signaling/ayame/ayame_channel.hpp>
#include <pc/peer_connection.hpp>

// boost
#include <boost/asio/io_context_strand.hpp>

using namespace naivertc;

class Client: public signaling::BaseChannel::Observer, public std::enable_shared_from_this<Client> {
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
    void CreatePeerConnection(const RtcConfiguration& rtc_config);

    void SendLocalSDP(const std::string& sdp, bool is_offer);
    void SendLocalCandidate(const std::string& mid, const std::string& sdp);

private:
    void OnConnected(bool is_initiator) override;
    void OnClosed(boost::system::error_code ec) override;
    void OnIceServers(std::vector<IceServer> ice_servers) override;
    void OnRemoteSDP(const std::string sdp, bool is_offer) override;
    void OnRemoteCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) override;

private:
    boost::asio::io_context& ioc_;
    boost::asio::io_context::strand strand_;

    std::unique_ptr<signaling::AyameChannel> ayame_channel_;
    std::shared_ptr<PeerConnection> peer_conn_;
};

#endif