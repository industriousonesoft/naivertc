#include "client.hpp"

#include <iostream>

#define HAS_MEDIA 1

template <class T> std::weak_ptr<T> make_weak_ptr(std::shared_ptr<T> ptr) { return ptr; }

void Client::CreatePeerConnection(const RtcConfiguration& rtc_config) {
    if (peer_conn_) {
        peer_conn_->Close();
        peer_conn_.reset();
    }

    peer_conn_ = PeerConnection::Create(rtc_config);

    peer_conn_->OnConnectionStateChanged([](PeerConnection::ConnectionState new_state){
        std::cout << "Peer connection state:" << new_state << std::endl;
    });

    peer_conn_->OnIceGatheringStateChanged([](PeerConnection::GatheringState new_state) {
        std::cout << "Peer gathering state: " << new_state << std::endl;
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
    // TODO: Generate a random 16-char and case-insensitive string, e.g.; TjtznXLCNH7nbRw
    std::string cname = "naivertc-media-cname";
    // TODO: Generate a random 36-char and case-insensitive string, e.g.; h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C
    std::string media_stream_id = "naivertc-media-stream-id";
    // Local video track
    MediaTrack::Configuration video_track_config("1", MediaTrack::Kind::VIDEO, MediaTrack::Codec::H264);
    // video_track_config.nack_enabled = true;
    // video_track_config.rtx_enabled = true;
    // video_track_config.fec_codec.emplace(MediaTrack::FecCodec::ULP_FEC)
    video_track_config.cname.emplace(cname);
    video_track_config.msid.emplace(media_stream_id);
    video_track_config.track_id.emplace("video-track-id-1");
    video_track_ = peer_conn_->AddTrack(video_track_config);
    video_track_->OnOpened([](){
        std::cout << "Local video track is opened.";
    });
    video_track_->OnClosed([](){
        std::cout << "Local video track is closed.";
    });

    // Local audio track
    MediaTrack::Configuration audio_track_config("2", MediaTrack::Kind::AUDIO, MediaTrack::Codec::OPUS);
    video_track_config.cname.emplace(cname);
    video_track_config.msid.emplace(media_stream_id);
    video_track_config.track_id.emplace("audio-track-id-1");

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