#ifndef _PC_PEER_CONNECTION_H_
#define _PC_PEER_CONNECTION_H_

#include "base/defines.hpp"
#include "base/proxy.hpp"
#include "pc/configuration.hpp"
#include "pc/candidate.hpp"
#include "pc/sdp_session_description.hpp"

#include <exception>

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

    using SDPCreateSuccessCallback = std::function<void(sdp::SessionDescription* sdp)>;
    using SDPCreateFailureCallback = std::function<void(const std::exception&)>;

    using SDPSetSuccessCallback = std::function<void()>;
    using SDPSetFailureCallback = std::function<void(const std::exception&)>;
public:
    static std::shared_ptr<PeerConnection> CreatePeerConnection(Configuration config) {
        return std::shared_ptr<PeerConnection>(new PeerConnection(config));
    }
    ~PeerConnection();

    void CreateOffer(SDPSetSuccessCallback on_success = nullptr, 
                    SDPCreateFailureCallback on_failure = nullptr);
    void CreateAnswer(SDPSetSuccessCallback on_success = nullptr, 
                    SDPCreateFailureCallback on_failure = nullptr);

    void SetOffer(const std::string sdp,
                SDPSetSuccessCallback on_success = nullptr, 
                SDPSetFailureCallback on_failure = nullptr);
    void SetAnswer(const std::string sdp, 
                SDPSetSuccessCallback on_success = nullptr, 
                SDPSetFailureCallback on_failure = nullptr);

    // setup State & Candidate callback
    void OnConnectionStateChanged(ConnectionStateCallback callback);
    void OnIceGatheringStateChanged(GatheringStateCallback callback);
    void OnIceCandidate(CandidateCallback callback);

protected:
    PeerConnection(Configuration config);
};

}

#endif