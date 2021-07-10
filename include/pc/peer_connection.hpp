#ifndef _PC_PEER_CONNECTION_H_
#define _PC_PEER_CONNECTION_H_

#include "base/defines.hpp"
#include "base/certificate.hpp"
#include "common/proxy.hpp"
#include "common/task_queue.hpp"
#include "pc/peer_connection_configuration.hpp"
#include "pc/sdp/candidate.hpp"
#include "pc/sdp/sdp_session_description.hpp"
#include "pc/transports/ice_transport.hpp"
#include "pc/transports/dtls_transport.hpp"
#include "pc/transports/sctp_transport.hpp"
#include "pc/media/media_track.hpp"
#include "pc/channels/data_channel.hpp"

#include <sigslot.h>

#include <exception>
#include <unordered_map>

namespace naivertc {

// PeerConnection
class RTC_CPP_EXPORT PeerConnection final : public sigslot::has_slots<>, 
                                            public std::enable_shared_from_this<PeerConnection> {
public:
    // ConnectionState
    enum class ConnectionState: int {
        NEW = 0,
        CONNECTING,
        CONNECTED,
        DISCONNECTED,
        FAILED,
        CLOSED
    };

    // GatheringState
    enum class GatheringState: int {
        NONE = -1,
        NEW = 0,
        GATHERING,
        COMPLETED
    };

    // SignalingState
    // See https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/signalingState
    enum class SignalingState: int {
        // 1. means the peer connection is new, and both the local and remote sdp are null
        // 2. means the negotiation is complete and a connection has been established
        STABLE = 0,
        HAVE_LOCAL_OFFER,
        HAVE_REMOTE_OFFER,
        HAVE_LOCAL_PRANSWER,
        HAVE_REMOTE_PRANSWER
    };

    using ConnectionStateCallback = std::function<void(ConnectionState new_state)>;
    using GatheringStateCallback = std::function<void(GatheringState new_state)>;
    using CandidateCallback = std::function<void(const Candidate& candidate)>;
    using SignalingStateCallback = std::function<void(SignalingState new_state)>;

    using SDPCreateSuccessCallback = std::function<void(sdp::SessionDescription sdp)>;
    using SDPCreateFailureCallback = std::function<void(const std::exception_ptr)>;

    using SDPSetSuccessCallback = std::function<void()>;
    using SDPSetFailureCallback = std::function<void(const std::exception_ptr)>;
public:
    static std::shared_ptr<PeerConnection> CreatePeerConnection(Configuration config) {
        return std::shared_ptr<PeerConnection>(new PeerConnection(config));
    }
    ~PeerConnection();

    std::shared_ptr<MediaTrack> AddTrack(const MediaTrack::Config& config);
    std::shared_ptr<DataChannel> CreateDataChannel(const DataChannel::Config& config);

    void CreateOffer(SDPCreateSuccessCallback on_success = nullptr, 
                    SDPCreateFailureCallback on_failure = nullptr);
    void CreateAnswer(SDPCreateSuccessCallback on_success = nullptr, 
                    SDPCreateFailureCallback on_failure = nullptr);

    void SetOffer(const std::string sdp,
                SDPSetSuccessCallback on_success = nullptr, 
                SDPSetFailureCallback on_failure = nullptr);
    void SetAnswer(const std::string sdp, 
                SDPSetSuccessCallback on_success = nullptr, 
                SDPSetFailureCallback on_failure = nullptr);

    void AddRemoteCandidate(const Candidate& candidate);

public:
    // setup State & Candidate callback
    void OnConnectionStateChanged(ConnectionStateCallback callback);
    void OnIceGatheringStateChanged(GatheringStateCallback callback);
    void OnIceCandidate(CandidateCallback callback);
    void OnSignalingStateChanged(SignalingStateCallback callback);

    static std::string signaling_state_to_string(SignalingState state);

protected:
    PeerConnection(Configuration config);

private:
    void InitIceTransport();
    void InitSctpTransport();

    bool UpdateConnectionState(ConnectionState state);
    bool UpdateGatheringState(GatheringState state);
    bool UpdateSignalingState(SignalingState state);

    void SetLocalDescription(sdp::Type type);
    void SetRemoteDescription(sdp::SessionDescription description);

    void ProcessLocalDescription(sdp::SessionDescription session_description);
    void ProcessRemoteDescription(sdp::SessionDescription session_description);
    void ValidRemoteDescription(const sdp::SessionDescription& session_description);
    void ProcessRemoteCandidate(Candidate candidate);

    void AddRemoteTrack(sdp::Media description);
  
    sdp::Media BuildMediaTrackDescription(const MediaTrack::Config& config);

private:
    // IceTransport callbacks
    void OnIceTransportStateChanged(Transport::State transport_state);
    void OnGatheringStateChanged(IceTransport::GatheringState gathering_state);
    void OnCandidateGathered(Candidate candidate);

    // SctpTransport callbacks
    void OnSctpTransportStateChanged(Transport::State transport_state);
    void OnBufferedAmountChanged(StreamId stream_id, size_t amount);
    void OnSctpPacketReceived(std::shared_ptr<Packet> in_packet);

private:
    TaskQueue handle_queue_;

    const Configuration config_;
    std::future<std::shared_ptr<Certificate>> certificate_;

    ConnectionState connection_state_;
    GatheringState gathering_state_;
    SignalingState signaling_state_;

    bool negotiation_needed_;

    std::shared_ptr<IceTransport> ice_transport_;
    std::shared_ptr<DtlsTransport> dtls_transport_;
    std::shared_ptr<SctpTransport> sctp_transport_;

    ConnectionStateCallback connection_state_callback_;
    GatheringStateCallback gathering_state_callback_;
    CandidateCallback candidate_callback_;
    SignalingStateCallback signaling_state_callback_;

    std::optional<sdp::SessionDescription> local_session_description_;
    std::optional<sdp::SessionDescription> remote_session_description_;

    // FIXME: Do we need to use shared_ptr instead of weak_ptr here?
    std::unordered_map<StreamId, std::weak_ptr<DataChannel>> data_channels_;
    std::unordered_map<std::string /* mid */, std::weak_ptr<MediaTrack>> media_tracks_;

    std::vector<const Candidate> remote_candidates_;

};

}

#endif