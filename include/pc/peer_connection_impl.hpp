#ifndef _PC_PEER_CONNECTION_IMPL_H_
#define _PC_PEER_CONNECTION_IMPL_H_

#include "base/defines.hpp"
#include "pc/configuration.hpp"
#include "pc/peer_connection.hpp"

#include <memory>

namespace naivertc {

class RTC_CPP_EXPORT PeerConnectionImpl : public std::enable_shared_from_this<PeerConnectionImpl> {
public:
    PeerConnectionImpl(Configuration config);
    ~PeerConnectionImpl();

    void set_connection_state_callback(PeerConnection::ConnectionStateCallback callback) { connection_state_callback_ = callback; }
    void set_gathering_state_callback(PeerConnection::GatheringStateCallback callback) { gathering_state_callback_ = callback; }
    void set_candidate_callback(PeerConnection::CandidateCallback callback) { candidate_callback_ = callback; }
   
private:
    const Configuration config_;

    PeerConnection::ConnectionStateCallback connection_state_callback_;
    PeerConnection::GatheringStateCallback gathering_state_callback_;
    PeerConnection::CandidateCallback candidate_callback_;
};

}

#endif