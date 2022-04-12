#include "client.hpp"

#include <plog/Log.h>

void Client::OnIceServers(std::vector<IceServer> ice_servers) {
    ioc_.post(strand_.wrap([this, ice_servers](){
        RtcConfiguration rtc_config;
        rtc_config.ice_servers = ice_servers;
        this->CreatePeerConnection(rtc_config);
    }));
}

void Client::OnConnected(bool is_initiator) {
    PLOG_DEBUG << " is_initiator: " << is_initiator;
    ioc_.post(strand_.wrap([this, is_initiator](){
        // If we are not initiator, we act as a offer
        if (!is_initiator) {
            this->peer_conn_->CreateOffer([this](const sdp::Description local_sdp){
                PLOG_VERBOSE << "Did create local offer sdp: " << std::string(local_sdp) ;
                this->SendLocalSDP(local_sdp, true);
            }, [this](const std::exception& exp){
                PLOG_VERBOSE << "Failed to create offer: " << exp.what();
                this->peer_conn_->Close();
            });
        }
    }));
}

void Client::OnClosed(const std::string reason) {
    PLOG_DEBUG << "Signaling channel did close: " << reason <<std::endl;
    ioc_.post(strand_.wrap([this](){
        if (this->peer_conn_) {
            this->peer_conn_->Close();
        }
    }));
}

void Client::OnRemoteSDP(const std::string remote_sdp, bool is_offer) {
    ioc_.post(strand_.wrap([this, remote_sdp, is_offer](){
        if (is_offer) {
            this->peer_conn_->SetOffer(remote_sdp, [this](){
                PLOG_VERBOSE << "Did set remote offer " ;
                this->peer_conn_->CreateAnswer([this](const sdp::Description& local_sdp){
                    PLOG_VERBOSE << "Did create local answer sdp" ;
                    this->SendLocalSDP(local_sdp, false);
                }, [this](const std::exception& exp){
                    PLOG_VERBOSE << "Failed to create answer: " << exp.what();
                    this->peer_conn_->Close();
                });
            }, [this](const std::exception& exp){
                PLOG_VERBOSE << "Failed to set remote offer: " << exp.what();
                this->peer_conn_->Close();
            });
        }else {
            this->peer_conn_->SetAnswer(remote_sdp, [](){
                PLOG_VERBOSE << "Did set remote answer ";
            }, [this](const std::exception& exp){
                PLOG_VERBOSE << "Failed to set remote answer: " << exp.what();
                this->peer_conn_->Close();
            });
        }
    }));
}

void Client::OnRemoteCandidate(const std::string sdp_mid, const int sdp_mlineindex, const std::string candidate) {
    ioc_.post(strand_.wrap([this, sdp_mid, candidate, sdp_mlineindex](){
        PLOG_DEBUG << ": Remote candidate => mid: " << sdp_mid 
                   << " lineindex: " << sdp_mlineindex 
                   << " sdp: " << candidate;
        this->peer_conn_->AddRemoteCandidate(sdp_mid, candidate);
    }));
}