#include "client.hpp"

#include <iostream>

void Client::OnIceServers(std::vector<IceServer> ice_servers) {
    std::cout << __FUNCTION__ << std::endl;
    ioc_.post(strand_.wrap([this, ice_servers = std::move(ice_servers)](){

        RtcConfiguration rtc_config;
        rtc_config.ice_servers = std::move(ice_servers);

        this->CreatePeerConnection(rtc_config);

    }));
}

void Client::OnConnected(bool is_initiator) {
    std::cout << __FUNCTION__ << " is_initiator: " << is_initiator << std::endl;
    ioc_.post(strand_.wrap([this, is_initiator](){
        // If we are not initiator, we act as a offer
        if (!is_initiator) {
            this->peer_conn_->CreateOffer([this](const sdp::Description& local_sdp){
                std::cout << "Did create local offer sdp " << std::endl;
                this->SendLocalSDP(std::move(local_sdp), true);
            }, [this](const std::exception& exp){
                std::cout << "Failed to create offer: " << exp.what() << std::endl;
                this->peer_conn_->Close();
            });
        }
    }));
}

void Client::OnClosed(boost::system::error_code ec) {
    std::cout << __FUNCTION__ << ": Signaling channel did close: " << ec.message() <<std::endl;
    ioc_.post(strand_.wrap([this](){
        this->peer_conn_->Close();
    }));
}

void Client::OnRemoteSDP(const std::string sdp, bool is_offer) {
    ioc_.post(strand_.wrap([this, remote_sdp = std::move(sdp), is_offer](){
        if (is_offer) {
            this->peer_conn_->SetOffer(std::move(remote_sdp), [this](){
                std::cout << "Did set remote offer " << std::endl;
                this->peer_conn_->CreateAnswer([this](const sdp::Description& local_sdp){
                    std::cout << "Did create local answer sdp" << std::endl;
                    this->SendLocalSDP(std::move(local_sdp), false);
                }, [this](const std::exception& exp){
                    std::cout << "Failed to create answer: " << exp.what() << std::endl;
                    this->peer_conn_->Close();
                });
            }, [this](const std::exception& exp){
                std::cout << "Failed to set remote offer: " << exp.what() << std::endl;
                this->peer_conn_->Close();
            });
        }else {
            this->peer_conn_->SetAnswer(std::move(remote_sdp), [](){
                std::cout << "Did set remote answer " << std::endl;
            }, [this](const std::exception& exp){
                std::cout << "Failed to set remote answer: " << exp.what() << std::endl;
                this->peer_conn_->Close();
            });
        }
    }));
}

void Client::OnRemoteCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) {
    ioc_.post(strand_.wrap([this, mid = std::move(sdp_mid), sdp = std::move(candidate)](){
        this->peer_conn_->AddRemoteCandidate(std::move(mid), std::move(sdp));
    }));
}