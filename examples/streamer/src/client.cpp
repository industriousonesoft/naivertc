#include "client.hpp"

Client::Client(boost::asio::io_context& ioc) 
    : ioc_(ioc) {
    ayame_channel_.reset(new naivertc::signaling::AyameChannel(ioc, weak_from_this()));
}

Client::~Client() {

}

void Client::Start() {
    naivertc::signaling::Config signaling_config;
    signaling_config.insecure = true;
    signaling_config.signaling_url = "wss://ayame-labo.shiguredo.jp/signaling";
    signaling_config.room_id = "industriousonesoft@ayame-labo-sample";
    signaling_config.client_id = "horseman-naive-rtc";
    signaling_config.signaling_key = "dzSU5Lz88dfZ0mVTWp51X8bPKBzfmhfdZH8D2ei3U7aNplX6";

    ayame_channel_->Connect(signaling_config);
}

void Client::Stop() {
    ayame_channel_->Close();
}

void Client::CreatePeerConnection(const naivertc::RtcConfiguration& rtc_config) {
    peer_conn_ = naivertc::PeerConnection::Create(std::move(rtc_config));

    
}

// Signaling channel observer
void Client::OnConnected(bool is_initiator) {

}

void Client::OnClosed(boost::system::error_code ec) {

}

void Client::OnIceServers(std::vector<naivertc::IceServer> ice_servers) {
    ioc_.post([this, ice_servers = std::move(ice_servers)](){

        naivertc::RtcConfiguration rtc_config;
        rtc_config.ice_servers = std::move(ice_servers);

        this->CreatePeerConnection(std::move(rtc_config));

    });
}

void Client::OnRemoteSDP(const std::string sdp, bool is_offer) {

}

void Client::OnRemoteCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) {

}