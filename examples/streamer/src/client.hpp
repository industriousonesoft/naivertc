#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "stream/h264_file_stream_source.hpp"

// signaling
#include <channels/ayame_client.hpp>
// naivertc
#include <rtc/pc/peer_connection.hpp>
#include <rtc/base/task_utils/task_queue.hpp>

// boost
#include <boost/asio/io_context_strand.hpp>

using namespace naivertc;

class Client: public signaling::AyameClient::Observer {
    Client(boost::asio::io_context& ioc);
public:
    static std::shared_ptr<Client> Create(boost::asio::io_context& ioc) {
        return std::shared_ptr<Client>(new Client(ioc));
    }
    // shared_ptr在引用为0是需要销毁对象，因此需确保析构函数是公开的
    ~Client() override;

    void Start();
    void Stop();

private:
    void CreatePeerConnection(const RtcConfiguration& rtc_config);
    
    void AddAudioTrack(std::string cname, std::string stream_id);
    void AddVideoTrack(std::string cname, std::string stream_id);
    void AddDataChannel();

    void SendLocalSDP(const std::string sdp, bool is_offer);
    void SendLocalCandidate(const std::string mid, const std::string sdp);

    void StartVideoStream(MediaStreamSource::SampleAvailableCallback callback);
    void StopVideoStream();

private:
    // Implements signaling::AyameClient::Observer
    void OnConnected(bool is_initiator) override;
    void OnClosed(const std::string err_reason) override;
    void OnIceServers(std::vector<IceServer> ice_servers) override;
    void OnRemoteSDP(const std::string sdp, bool is_offer) override;
    void OnRemoteCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) override;

private:
    boost::asio::io_context& ioc_;
    boost::asio::io_context::strand strand_;

    std::unique_ptr<signaling::AyameClient> ayame_client_;
    std::shared_ptr<PeerConnection> peer_conn_;

    std::shared_ptr<DataChannel> data_channel_;
    std::shared_ptr<AudioTrack> audio_track_;
    std::shared_ptr<VideoTrack> video_track_;

    std::unique_ptr<naivertc::TaskQueue> worker_queue_;
    std::unique_ptr<H264FileStreamSource> h264_file_stream_source_;
};

#endif