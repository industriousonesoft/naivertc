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
    signaling::Configuration signaling_config;
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
