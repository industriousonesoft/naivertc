#include "pc/peer_connection.hpp"
#include "pc/peer_connection_impl.hpp"

namespace naivertc {

PeerConnection::PeerConnection(Configuration config) 
    : Proxy<PeerConnectionImpl>(std::move(config) /* std::move确保传入的参数是右值 */) {

}

PeerConnection::~PeerConnection() {

}

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