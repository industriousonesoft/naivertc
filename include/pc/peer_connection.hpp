#ifndef _PC_PEER_CONNECTION_H_
#define _PC_PEER_CONNECTION_H_

#include "base/defines.hpp"
#include "base/proxy.hpp"
#include "pc/configuration.hpp"
#include "pc/candidate.hpp"

namespace naivertc {

class PeerConnectionImpl;

// PeerConnection
class RTC_CPP_EXPORT PeerConnection final : Proxy<PeerConnectionImpl> {
public:
    // ConnectionState
    enum class ConnectionState {
        NEW = 0,
        CONNECTING,
        CONNECTED,
        DISCONNECTED,
        FAILED,
        CLOSED
    };

    // GatheringState
    enum class GatheringState {
        NEW = 0,
        GATHERING,
        COMPLETED
    };

    using ConnectionStateCallback = std::function<void(ConnectionState new_state)>;
    using GatheringStateCallback = std::function<void(GatheringState new_state)>;
    using CandidateCallback = std::function<void(Candidate* candidate)>;

    // using SDPCreateSuccessCallback = std::function<void()>
public:
    static std::shared_ptr<PeerConnection> CreatePeerConnection(Configuration config) {
        return std::shared_ptr<PeerConnection>(new PeerConnection(config));
    }
    ~PeerConnection();

    // setup State & Candidate callback
    void OnConnectionStateChanged(ConnectionStateCallback callback);
    void OnIceGatheringStateChanged(GatheringStateCallback callback);
    void OnIceCandidate(CandidateCallback callback);

protected:
    PeerConnection(Configuration config);

};

}

#endif