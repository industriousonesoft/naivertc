#ifndef _PC_PEER_CONNECTION_H_
#define _PC_PEER_CONNECTION_H_

#include "base/defines.hpp"
#include "base/proxy.hpp"
#include "base/task_queue.hpp"
#include "pc/configuration.hpp"
#include "pc/candidate.hpp"
#include "pc/sdp_session_description.hpp"
#include "pc/ice_transport.hpp"

#include <sigslot.h>

#include <exception>

namespace naivertc {

// PeerConnection
class RTC_CPP_EXPORT PeerConnection final : public sigslot::has_slots<>, 
                                            public std::enable_shared_from_this<PeerConnection> {
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
    using CandidateCallback = std::function<void(Candidate candidate)>;

    using SDPCreateSuccessCallback = std::function<void(sdp::SessionDescription* sdp)>;
    using SDPCreateFailureCallback = std::function<void(const std::exception& exp)>;

    using SDPSetSuccessCallback = std::function<void()>;
    using SDPSetFailureCallback = std::function<void(const std::exception& exp)>;
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

    void OnTransportStateChanged(Transport::State transport_state);
    void OnGatheringStateChanged(IceTransport::GatheringState gathering_state);
    void OnCandidateGathered(Candidate candidate);

protected:
    PeerConnection(Configuration config);

private:
    void InitIceTransport();
    bool UpdateConnectionState(ConnectionState state);
    bool UpdateGatheringState(GatheringState state);

private:
    TaskQueue handle_queue_;

    const Configuration config_;
    ConnectionState connection_state_;
    GatheringState gathering_state_;

    std::unique_ptr<IceTransport> ice_transport_;

    PeerConnection::ConnectionStateCallback connection_state_callback_;
    PeerConnection::GatheringStateCallback gathering_state_callback_;
    PeerConnection::CandidateCallback candidate_callback_;
};

}

#endif