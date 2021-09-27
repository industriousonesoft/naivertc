#include "client.hpp"

#include <plog/Log.h>

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
        PLOG_INFO << "Peer connection state:" << new_state;
    });

    peer_conn_->OnIceGatheringStateChanged([](PeerConnection::GatheringState new_state) {
        PLOG_INFO << "Peer gathering state: " << new_state;
    });

    peer_conn_->OnIceCandidate([this](const sdp::Candidate& candidate){
        auto mid = candidate.mid();
        auto sdp = std::string(candidate);
        PLOG_INFO << "Local candidate => mid: " << mid << " sdp: " << sdp;
        ioc_.post(strand_.wrap([this, mid = std::move(mid), sdp = std::move(sdp)](){
            this->SendLocalCandidate(mid, sdp);
        }));
    });

    peer_conn_->OnDataChannel([](std::shared_ptr<DataChannel> data_channel){
        PLOG_INFO << "Remote data channel: " << data_channel->label();
    }); 

#if HAS_MEDIA
    // TODO: Generate a random 16-char and case-insensitive string, e.g.; TjtznXLCNH7nbRw
    std::string cname = "naivertc-media-cname";
    // TODO: Generate a random 36-char and case-insensitive string, e.g.; h1aZ20mbQB0GSsq0YxLfJmiYWE9CBfGch97C
    std::string media_stream_id = "naivertc-media-stream-id";

    // Local audio track
    MediaTrack::Configuration audio_track_config(MediaTrack::Kind::AUDIO, "1");
    audio_track_config.direction = MediaTrack::Direction::SEND_ONLY;
    audio_track_config.AddCodec(MediaTrack::Codec::OPUS);

    audio_track_config.cname.emplace(cname);
    audio_track_config.msid.emplace(media_stream_id);
    audio_track_config.track_id.emplace("audio-track-id-1");
    
    audio_track_ = peer_conn_->AddTrack(audio_track_config);
    audio_track_->OnOpened([](){
        PLOG_INFO << "Local audio track is opened.";
    });
    audio_track_->OnClosed([](){
        PLOG_INFO << "Local audio track is closed.";
    });

    // Local video track
    MediaTrack::Configuration video_track_config(MediaTrack::Kind::VIDEO, "2");
    video_track_config.direction = MediaTrack::Direction::SEND_ONLY;
    video_track_config.rtx_enabled = true;
    video_track_config.fec_codec.emplace(MediaTrack::FecCodec::ULP_FEC);
    video_track_config.AddCodec(MediaTrack::Codec::H264);
    video_track_config.AddFeedback(MediaTrack::RtcpFeedback::NACK);

    video_track_config.cname.emplace(cname);
    video_track_config.msid.emplace(media_stream_id);
    video_track_config.track_id.emplace("video-track-id-1");

    video_track_ = peer_conn_->AddTrack(video_track_config);
    video_track_->OnOpened([](){
        PLOG_INFO << "Local video track is opened.";
    });
    video_track_->OnClosed([](){
        PLOG_INFO << "Local video track is closed.";
    });
#endif

    // Data channel
    DataChannel::Init data_channel_init("naivertc-chat-data-channel");
    data_channel_ = peer_conn_->CreateDataChannel(data_channel_init);

    data_channel_->OnOpened([weak_dc=make_weak_ptr(data_channel_)](){
        if (auto dc = weak_dc.lock()) {
            PLOG_INFO << "OnOpened : " << weak_dc.lock()->label();
        }
    });

    data_channel_->OnClosed([weak_dc=make_weak_ptr(data_channel_)](){
        if (auto dc = weak_dc.lock()) {
            PLOG_INFO << "OnClosed : " << weak_dc.lock()->label();
        }
    });

    data_channel_->OnMessageReceived([weak_dc=make_weak_ptr(data_channel_)](const std::string text){
        PLOG_VERBOSE << "OnTextMessageReceived : " << text;
        if (auto dc = weak_dc.lock()) {
            auto res = "Hi, " + text;
            PLOG_VERBOSE << "Response: " << res;
            dc->Send(res);
        }
    });

    data_channel_->OnMessageReceived([](const uint8_t* in_data, size_t in_size){
        PLOG_VERBOSE << "OnBinaryMessageReceived : " << in_size;
    });

    data_channel_->OnBufferedAmountChanged([](uint64_t previous_amount){
        PLOG_VERBOSE << "OnBufferedAmountChanged : " << previous_amount;
    });

    // Incoming data channel
    peer_conn_->OnDataChannel([](std::shared_ptr<DataChannel> data_channel){
        PLOG_INFO << "Incoming data channel:" << data_channel->stream_id();
    });

    // Incoming media track
    peer_conn_->OnMediaTrack([](std::shared_ptr<MediaTrack> media_track){
        PLOG_INFO << "Incoming media track:" << media_track->mid();
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