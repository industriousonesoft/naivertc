#include "pc/peer_connection.hpp"
#include "pc/peer_connection_impl.hpp"

namespace naivertc {

PeerConnection::PeerConnection(Configuration config) 
    : Proxy<PeerConnectionImpl>(std::move(config) /* std::move确保传入的参数是右值 */) {

}

PeerConnection::~PeerConnection() {

}

// Offer && Answer
void PeerConnection::CreateOffer(SDPSetSuccessCallback on_success, 
                SDPCreateFailureCallback on_failure) {
    // call impl function in task queue maybe?
}

void PeerConnection::CreateAnswer(SDPSetSuccessCallback on_success, 
                SDPCreateFailureCallback on_failure) {

}

void PeerConnection::SetOffer(const std::string sdp,
            SDPSetSuccessCallback on_success,
            SDPSetFailureCallback on_failure) {

}

void PeerConnection::SetAnswer(const std::string sdp, 
            SDPSetSuccessCallback on_success, 
            SDPSetFailureCallback on_failure) {

}

// state && candidate callback
void PeerConnection::OnConnectionStateChanged(ConnectionStateCallback callback) {
    impl()->set_connection_state_callback(callback);
}

void PeerConnection::OnIceGatheringStateChanged(GatheringStateCallback callback) {
    impl()->set_gathering_state_callback(callback);
}

void PeerConnection::OnIceCandidate(CandidateCallback callback) {
    impl()->set_candidate_callback(callback);
}
    
}