#include "client.hpp"

#include <iostream>

#define HAS_MEDIA 0

template <class T> std::weak_ptr<T> make_weak_ptr(std::shared_ptr<T> ptr) { return ptr; }

void Client::CreatePeerConnection(const RtcConfiguration& rtc_config) {
    if (peer_conn_) {
        peer_conn_->Close();
        peer_conn_.reset();
    }

    peer_conn_ = PeerConnection::Create(rtc_config);

    peer_conn_->OnConnectionStateChanged([](PeerConnection::ConnectionState new_state){
        std::string conn_state_str = "Peer connection state: ";
        switch (new_state)
        {
        case PeerConnection::ConnectionState::NEW:
            conn_state_str += "New";
            break;
        case PeerConnection::ConnectionState::CONNECTING:
            conn_state_str += "Connecting";
            break;
        case PeerConnection::ConnectionState::CONNECTED:
            conn_state_str += "Connected";
            break;
        case PeerConnection::ConnectionState::DISCONNECTED:
            conn_state_str += "Disconnected";
            break;
        case PeerConnection::ConnectionState::FAILED:
            conn_state_str += "Failed";
            break;
        case PeerConnection::ConnectionState::CLOSED:
            conn_state_str += "Closed";
            break;
        default:
            break;
        }
        std::cout << conn_state_str << std::endl;
    });

    peer_conn_->OnIceGatheringStateChanged([](PeerConnection::GatheringState new_state) {
        std::string conn_state_str = "Peer gathering state: ";
        switch (new_state)
        {
        case PeerConnection::GatheringState::NEW:
            conn_state_str += "New";
            break;
        case PeerConnection::GatheringState::GATHERING:
            conn_state_str += "Gathering";
            break;
        case PeerConnection::GatheringState::COMPLETED:
            conn_state_str += "Completed";
            break;
        default:
            break;
        }
        std::cout << conn_state_str << std::endl;
    });

    peer_conn_->OnIceCandidate([this](const sdp::Candidate& candidate){
        auto mid = candidate.mid();
        auto sdp = std::string(candidate);
        std::cout << "Local candidate => mid: " << mid << " sdp: " << sdp << std::endl;
        ioc_.post(strand_.wrap([this, mid = std::move(mid), sdp = std::move(sdp)](){
            this->SendLocalCandidate(mid, sdp);
        }));
    });

    peer_conn_->OnDataChannel([](std::shared_ptr<DataChannel> data_channel){
        std::cout << "Remote data channel: " << data_channel->label() << std::endl;
    }); 

#if HAS_MEDIA
    std::string media_stream_id = "naivertc-media-stream";
    // Local video track
    MediaTrack::Config video_track_config("1", MediaTrack::Kind::VIDEO, MediaTrack::Codec::H264, {102}, 1, "video-stream", media_stream_id, "video-track1");
    video_track_ = peer_conn_->AddTrack(video_track_config);
    video_track_->OnOpened([](){
        std::cout << "Local video track is opened.";
    });
    video_track_->OnClosed([](){
        std::cout << "Local video track is closed.";
    });

    // Local audio track
    MediaTrack::Config audio_track_config("2", MediaTrack::Kind::AUDIO, MediaTrack::Codec::OPUS, {111}, 2, "audio-stream", media_stream_id, "audio-track1");
    audio_track_ = peer_conn_->AddTrack(audio_track_config);
    audio_track_->OnOpened([](){
        std::cout << "Local audio track is opened.";
    });
    audio_track_->OnClosed([](){
        std::cout << "Local audio track is closed.";
    });
#endif

    // Data channel
    DataChannel::Init data_channel_init("naivertc-chat-data-channel");
    data_channel_ = peer_conn_->CreateDataChannel(data_channel_init);

    data_channel_->OnOpened([weak_dc=make_weak_ptr(data_channel_)](){
        if (auto dc = weak_dc.lock()) {
            std::cout << "OnOpened : " << weak_dc.lock()->label() << std::endl;
        }
    });

    data_channel_->OnClosed([weak_dc=make_weak_ptr(data_channel_)](){
        if (auto dc = weak_dc.lock()) {
            std::cout << "OnClosed : " << weak_dc.lock()->label() << std::endl;
        }
    });

    data_channel_->OnMessageReceived([weak_dc=make_weak_ptr(data_channel_)](const std::string text){
        std::cout << "OnTextMessageReceived : " << text << std::endl;
        if (auto dc = weak_dc.lock()) {
            auto res = "Hi, " + text;
            std::cout << "Response: " << res << std::endl;
            dc->Send(res);
        }
    });

    data_channel_->OnMessageReceived([](const uint8_t* in_data, size_t in_size){
        std::cout << "OnBinaryMessageReceived : " << in_size << std::endl;
    });

    data_channel_->OnBufferedAmountChanged([](uint64_t previous_amount){
        std::cout << "OnBufferedAmountChanged : " << previous_amount << std::endl;
    });

    // Incoming data channel
    peer_conn_->OnDataChannel([](std::shared_ptr<DataChannel> data_channel){
        std::cout << "Incoming data channel:" << data_channel->stream_id();
    });

    // Incoming media track
    peer_conn_->OnMediaTrack([](std::shared_ptr<MediaTrack> media_track){
        std::cout << "Incoming media track:" << media_track->mid();
    });

}

void Client::SendLocalSDP(const std::string sdp, bool is_offer) {
    if (ayame_channel_) {
        ayame_channel_->SendLocalSDP(sdp, is_offer);
    }
}

void Client::SendLocalCandidate(const std::string mid, const std::string sdp) {
    if (ayame_channel_) {
        ayame_channel_->SendLocalCandidate(mid, 0, sdp);
    }
}