#include "client.hpp"

#include <iostream>

Client::Client(boost::asio::io_context& ioc) 
    : ioc_(ioc), 
    strand_(ioc_) {}

Client::~Client() {
    ayame_channel_.reset();
}

void Client::Start() {
    if (ayame_channel_ == nullptr) {
        ayame_channel_.reset(new signaling::AyameChannel(ioc_, weak_from_this()));
    }
    signaling::Config signaling_config;
    signaling_config.insecure = true;
    signaling_config.signaling_url = "wss://ayame-labo.shiguredo.jp/signaling";
    signaling_config.room_id = "industriousonesoft@ayame-labo-sample";
    signaling_config.client_id = "horseman-naive-rtc";
    signaling_config.signaling_key = "dzSU5Lz88dfZ0mVTWp51X8bPKBzfmhfdZH8D2ei3U7aNplX6";

    ayame_channel_->Connect(signaling_config);
}

void Client::Stop() {
    if (ayame_channel_) {
        ayame_channel_->Close();
    }
    if (peer_conn_) {
        peer_conn_->Close();
    }
}

void Client::CreatePeerConnection(const RtcConfiguration& rtc_config) {

    if (peer_conn_) {
        peer_conn_->Close();
        peer_conn_.reset();
    }

    peer_conn_ = PeerConnection::Create(std::move(rtc_config));

    std::string media_stream_id = "naivertc-media-stream";
    // Video track
    MediaTrack::Config video_track_config("1", MediaTrack::Kind::VIDEO, MediaTrack::Codec::H264, {102}, 1, "video-stream", media_stream_id, "video-track1");
    peer_conn_->AddTrack(std::move(video_track_config));

    // Audio track
    MediaTrack::Config audio_track_config("2", MediaTrack::Kind::AUDIO, MediaTrack::Codec::OPUS, {111}, 2, "audio-stream", media_stream_id, "audio-track1");
    peer_conn_->AddTrack(std::move(audio_track_config));

    // Data channel
    DataChannel::Config data_channel_config("chat-data-channel");
    peer_conn_->CreateDataChannel(std::move(data_channel_config));

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

    peer_conn_->OnIceCandidate([this](const Candidate& candidate){
        std::cout << "Local candidate: " << std::string(candidate) << std::endl;
        auto mid = candidate.mid();
        auto sdp = std::string(candidate);
        ioc_.post(strand_.wrap([this, mid = std::move(mid), sdp = std::move(sdp)](){
            this->SendLocalCandidate(std::move(mid), std::move(sdp));
        }));
    });

}

void Client::SendLocalSDP(const std::string& sdp, bool is_offer) {
    if (ayame_channel_) {
        ayame_channel_->SendLocalSDP(std::move(sdp), is_offer);
    }
}

void Client::SendLocalCandidate(const std::string& mid, const std::string& sdp) {
    if (ayame_channel_) {
        ayame_channel_->SendLocalCandidate(std::move(mid), 0, std::move(sdp));
    }
}
