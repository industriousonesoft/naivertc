#ifndef _RTC_PEER_CONNECTION_H_
#define _RTC_PEER_CONNECTION_H_

#include "base/defines.hpp"
#include "base/certificate.hpp"
#include "common/proxy.hpp"
#include "common/task_queue.hpp"
#include "rtc/pc/peer_connection_configuration.hpp"
#include "rtc/sdp/candidate.hpp"
#include "rtc/sdp/sdp_description.hpp"
#include "rtc/transports/ice_transport.hpp"
#include "rtc/transports/dtls_transport.hpp"
#include "rtc/transports/sctp_transport.hpp"
#include "rtc/media/media_track.hpp"
#include "rtc/channels/data_channel.hpp"
#include "rtc/rtp_rtcp/rtp/packets/rtp_packet.hpp"


#include <exception>
#include <unordered_map>

namespace naivertc {

// PeerConnection
class RTC_CPP_EXPORT PeerConnection final : public std::enable_shared_from_this<PeerConnection> {
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
    using CandidateCallback = std::function<void(const sdp::Candidate& candidate)>;
    using SignalingStateCallback = std::function<void(SignalingState new_state)>;
    using DataChannelCallback = std::function<void(std::shared_ptr<DataChannel>)>;

    using SDPCreateSuccessCallback = std::function<void(const sdp::Description sdp)>;
    using SDPCreateFailureCallback = std::function<void(const std::exception exp)>;

    using SDPSetSuccessCallback = std::function<void()>;
    using SDPSetFailureCallback = std::function<void(const std::exception exp)>;
public:
    static std::shared_ptr<PeerConnection> Create(const RtcConfiguration config) {
        return std::shared_ptr<PeerConnection>(new PeerConnection(std::move(config)));
    }
    ~PeerConnection();

    std::shared_ptr<MediaTrack> AddTrack(const MediaTrack::Configuration config);
    std::shared_ptr<DataChannel> CreateDataChannel(const DataChannel::Init config);

    void CreateOffer(SDPCreateSuccessCallback on_success = nullptr, 
                    SDPCreateFailureCallback on_failure = nullptr);
    void CreateAnswer(SDPCreateSuccessCallback on_success = nullptr, 
                    SDPCreateFailureCallback on_failure = nullptr);

    // Passing 'sdp' by value other than reference in a async method
    void SetOffer(const std::string sdp,
                SDPSetSuccessCallback on_success = nullptr, 
                SDPSetFailureCallback on_failure = nullptr);
    void SetAnswer(const std::string sdp, 
                SDPSetSuccessCallback on_success = nullptr, 
                SDPSetFailureCallback on_failure = nullptr);

    void AddRemoteCandidate(const std::string mid, const std::string sdp);

    void Close();

public:
    // setup State & Candidate callback
    void OnConnectionStateChanged(ConnectionStateCallback callback);
    void OnIceGatheringStateChanged(GatheringStateCallback callback);
    void OnIceCandidate(CandidateCallback callback);
    void OnSignalingStateChanged(SignalingStateCallback callback);
    void OnDataChannel(DataChannelCallback callback);

    static std::string signaling_state_to_string(SignalingState state);

protected:
    PeerConnection(const RtcConfiguration config);

private:
    void InitIceTransport();
    void InitDtlsTransport();
    void InitSctpTransport();

    bool UpdateConnectionState(ConnectionState state);
    bool UpdateGatheringState(GatheringState state);
    bool UpdateSignalingState(SignalingState state);

    void SetLocalDescription(sdp::Type type);
    void SetRemoteDescription(sdp::Description remote_sdp);

    void ProcessLocalDescription(sdp::Description local_sdp);
    void ProcessRemoteDescription(sdp::Description remote_sdp);
    void ValidRemoteDescription(const sdp::Description& remote_sdp);

    void TryToGatherLocalCandidate();
    void ProcessRemoteCandidates();
    void ProcessRemoteCandidate(sdp::Candidate candidate);
    void AddRemoteCandidate(const sdp::Candidate& candidate);

    void AddReciprocatedMediaTrack(sdp::Media media_sdp);
    void ShiftDataChannelIfNeccessary(sdp::Role role);

    void ResetCallbacks();
    void CloseTransports();

    // DataChannel
    void OpenDataChannels();
    void CloseDataChannels();
    void RemoteCloseDataChannels();
    void OnRemoteDataChannelOpened(std::weak_ptr<DataChannel> data_channel);
    std::shared_ptr<DataChannel> FindDataChannel(StreamId stream_id);
  
private:
    // IceTransport callbacks
    void OnIceTransportStateChanged(Transport::State transport_state);
    void OnGatheringStateChanged(IceTransport::GatheringState gathering_state);
    void OnCandidateGathered(sdp::Candidate candidate);
    void OnRoleChanged(sdp::Role role);

    // DtlsTransport callbacks
    void OnDtlsTransportStateChanged(DtlsTransport::State transport_state);
    bool OnDtlsVerify(std::string_view fingerprint);
    void OnRtpPacketReceived(std::shared_ptr<RtpPacket> in_packet);

    // SctpTransport callbacks
    void OnSctpTransportStateChanged(SctpTransport::State transport_state);
    void OnBufferedAmountChanged(StreamId stream_id, size_t amount);
    void OnSctpMessageReceived(std::shared_ptr<Packet> in_packet);

private:
    const RtcConfiguration rtc_config_;
    std::shared_future<std::shared_ptr<Certificate>> certificate_;

    ConnectionState connection_state_ = ConnectionState::CLOSED;
    GatheringState gathering_state_ = GatheringState::NEW;
    SignalingState signaling_state_ = SignalingState::STABLE;

    bool negotiation_needed_ = false;

    std::unique_ptr<TaskQueue> signal_task_queue_ = nullptr;
    std::shared_ptr<TaskQueue> network_task_queue_ = nullptr;
    std::unique_ptr<TaskQueue> work_task_queue_ = nullptr;

    std::shared_ptr<IceTransport> ice_transport_ = nullptr;
    std::shared_ptr<DtlsTransport> dtls_transport_ = nullptr;
    std::shared_ptr<SctpTransport> sctp_transport_ = nullptr;

    ConnectionStateCallback connection_state_callback_ = nullptr;
    GatheringStateCallback gathering_state_callback_ = nullptr;
    CandidateCallback candidate_callback_ = nullptr;
    SignalingStateCallback signaling_state_callback_ = nullptr;
    DataChannelCallback data_channel_callback_ = nullptr;

    std::optional<sdp::Description> local_sdp_ = std::nullopt;
    std::optional<sdp::Description> remote_sdp_ = std::nullopt;

    // Using std::shared_ptr instead of std::weak_ptr to make sure them still valid during the peer connection
    std::unordered_map<StreamId, std::shared_ptr<DataChannel>> data_channels_;
    std::unordered_map<std::string /* mid */, std::shared_ptr<MediaTrack>> media_tracks_;

    std::vector<const sdp::Candidate> remote_candidates_;
    // TODO:  Using std::shared_ptr instead of std::weak_ptr
    std::vector<std::weak_ptr<DataChannel>> pending_data_channels_;

};

}

#endif