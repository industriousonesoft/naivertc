#ifndef _PC_PEER_CONNECTION_H_
#define _PC_PEER_CONNECTION_H_

#include "base/defines.hpp"
#include "common/proxy.hpp"
#include "common/task_queue.hpp"
#include "pc/peer_connection_configuration.hpp"
#include "pc/sdp/sdp_entry.hpp"
#include "pc/sdp/candidate.hpp"
#include "pc/sdp/sdp_session_description.hpp"
#include "pc/transports/ice_transport.hpp"
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
        NONE = -1,
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

    void AddTrack(std::shared_ptr<MediaTrack> media_track);
    void CreateDataChannel(DataChannelInit init);

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

public:
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
    void InitSctpTransport();

    bool UpdateConnectionState(ConnectionState state);
    bool UpdateGatheringState(GatheringState state);

    void SetLocalSessionDescription(sdp::SessionDescription session_description, 
                                    SDPSetSuccessCallback on_success = nullptr, 
                                    SDPSetFailureCallback on_failure = nullptr);
    void SetRemoteSessionDescription(sdp::SessionDescription session_description, 
                                    SDPSetSuccessCallback on_success = nullptr, 
                                    SDPSetFailureCallback on_failure = nullptr);

private:
    TaskQueue handle_queue_;

    const Configuration config_;
    ConnectionState connection_state_;
    GatheringState gathering_state_;

    std::shared_ptr<IceTransport> ice_transport_;
    std::shared_ptr<SctpTransport> sctp_transport_;

    PeerConnection::ConnectionStateCallback connection_state_callback_;
    PeerConnection::GatheringStateCallback gathering_state_callback_;
    PeerConnection::CandidateCallback candidate_callback_;

    std::optional<sdp::SessionDescription> local_session_description_;
    std::optional<sdp::SessionDescription> remote_session_description_;

    std::unordered_map<StreamId, std::shared_ptr<DataChannel>> data_channels_;
    std::unordered_map<std::string /* mid */, std::shared_ptr<sdp::Entry>> media_sdp_entries_;

};

}

#endif