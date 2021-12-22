#ifndef _RTC_PEER_CONNECTION_H_
#define _RTC_PEER_CONNECTION_H_

#include "base/defines.hpp"
#include "base/certificate.hpp"
#include "base/thread_annotation.hpp"
#include "common/proxy.hpp"
#include "rtc/base/time/clock_real_time.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/pc/peer_connection_configuration.hpp"
#include "rtc/sdp/candidate.hpp"
#include "rtc/sdp/sdp_description.hpp"
#include "rtc/transports/ice_transport.hpp"
#include "rtc/transports/dtls_transport.hpp"
#include "rtc/transports/sctp_transport.hpp"
#include "rtc/media/media_track.hpp"
#include "rtc/channels/data_channel.hpp"
#include "rtc/call/rtp_demuxer.hpp"

#include <exception>
#include <unordered_map>
#include <iostream>

namespace naivertc {

// PeerConnection
class RTC_CPP_EXPORT PeerConnection : public MediaTransport,
                                      public DataTransport {
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
    using MediaTrackCallback = std::function<void(std::shared_ptr<MediaTrack>)>;

    using SDPCreateSuccessCallback = std::function<void(const sdp::Description sdp)>;
    using SDPCreateFailureCallback = std::function<void(const std::exception& exp)>;

    using SDPSetSuccessCallback = std::function<void()>;
    using SDPSetFailureCallback = std::function<void(const std::exception& exp)>;

    static std::string ToString(ConnectionState state);
    static std::string ToString(GatheringState state);
    static std::string ToString(SignalingState state);
    
public:
    static std::shared_ptr<PeerConnection> Create(const RtcConfiguration& config) {
        return std::shared_ptr<PeerConnection>(new PeerConnection(config));
    }
    
public:
    ~PeerConnection() override;

    std::shared_ptr<MediaTrack> AddTrack(const MediaTrack::Configuration& config);
    std::shared_ptr<DataChannel> CreateDataChannel(const DataChannel::Init& config, std::optional<uint16_t> stream_id = std::nullopt);

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

    // setup State & Candidate callback
    void OnConnectionStateChanged(ConnectionStateCallback callback);
    void OnIceGatheringStateChanged(GatheringStateCallback callback);
    void OnIceCandidateGathered(CandidateCallback callback);
    void OnSignalingStateChanged(SignalingStateCallback callback);

    // Incoming data channel or media track created by remote peer
    void OnRemoteDataChannelReceived(DataChannelCallback callback);
    void OnRemoteMediaTrackReceived(MediaTrackCallback callback);

public:
    // MediaTransportInterface
    int SendRtpPacket(CopyOnWriteBuffer packet, PacketOptions options) override;
    // DataTransportInterface
    bool Send(SctpMessageToSend message) override;
    
protected:
    PeerConnection(const RtcConfiguration& config);

private:
    void InitIceTransport();
    void InitDtlsTransport();
    void InitSctpTransport();

    bool UpdateConnectionState(ConnectionState state);
    bool UpdateGatheringState(GatheringState state);
    bool UpdateSignalingState(SignalingState state);

    void ResetCallbacks();
    void CloseTransports();

    // SDP
    void SetLocalDescription(sdp::Type type);
    void SetRemoteDescription(sdp::Description remote_sdp);

    void ProcessLocalDescription(sdp::Description& local_sdp);
    void ProcessRemoteDescription(sdp::Description remote_sdp);
    void ValidRemoteDescription(const sdp::Description& remote_sdp);

    void ProcessRemoteCandidates();
    void ProcessRemoteCandidate(sdp::Candidate candidate);
    
    // DataChannel
    void OpenDataChannels();
    void CloseDataChannels();
    void RemoteCloseDataChannels();
    void FlushPendingDataChannels();
    void ShiftDataChannelIfNeccessary(sdp::Role role);
    void OnIncomingDataChannel(std::shared_ptr<DataChannel> data_channel);
    std::shared_ptr<DataChannel> FindDataChannel(uint16_t stream_id) const;

    // MediaTacks
    void OpenMediaTracks();
    void CloseMediaTracks();
    void FlushPendingMediaTracks();
    std::shared_ptr<MediaTrack> FindMediaTrack(std::string mid) const;
    void OnMediaTrackNegotiated(const sdp::Media& remote_sdp);
    void OnIncomingMediaTrack(const sdp::Media& remote_sdp);
  
private:
    // IceTransport callbacks
    void OnIceTransportStateChanged(Transport::State transport_state);
    void OnGatheringStateChanged(IceTransport::GatheringState gathering_state);
    void OnCandidateGathered(sdp::Candidate candidate);
    void OnRoleChanged(sdp::Role role);

    // DtlsTransport callbacks
    void OnDtlsTransportStateChanged(DtlsTransport::State transport_state);
    bool OnDtlsVerify(std::string_view fingerprint);
    void OnRtpPacketReceived(CopyOnWriteBuffer in_packet, bool is_rtcp);

    // SctpTransport callbacks
    void OnSctpTransportStateChanged(SctpTransport::State transport_state);
    void OnBufferedAmountChanged(uint16_t stream_id, size_t amount);
    void OnSctpMessageReceived(SctpMessage message);
    void OnSctpReadyToSend();
private:
    const RtcConfiguration rtc_config_ RTC_GUARDED_BY(signal_task_queue_);
    std::shared_future<std::shared_ptr<Certificate>> certificate_;
    
    ConnectionState connection_state_ RTC_GUARDED_BY(signal_task_queue_) = ConnectionState::CLOSED;
    GatheringState gathering_state_ RTC_GUARDED_BY(signal_task_queue_) = GatheringState::NEW;
    SignalingState signaling_state_ RTC_GUARDED_BY(signal_task_queue_) = SignalingState::STABLE;

    // Indicate if we need to negotiate or not.
    bool negotiation_needed_ RTC_GUARDED_BY(signal_task_queue_) = false;
    // Indicate if we need to create a data channel or not.
    bool data_channel_needed_ RTC_GUARDED_BY(signal_task_queue_) = false;

    std::unique_ptr<TaskQueue> signal_task_queue_ = nullptr;
    std::unique_ptr<TaskQueue> network_task_queue_ = nullptr;
    std::unique_ptr<TaskQueue> worker_task_queue_ = nullptr;

    std::unique_ptr<IceTransport> ice_transport_ RTC_GUARDED_BY(signal_task_queue_) = nullptr;
    std::unique_ptr<DtlsTransport> dtls_transport_ RTC_GUARDED_BY(signal_task_queue_) = nullptr;
    std::unique_ptr<SctpTransport> sctp_transport_ RTC_GUARDED_BY(signal_task_queue_) = nullptr;

    ConnectionStateCallback connection_state_callback_ RTC_GUARDED_BY(signal_task_queue_) = nullptr;
    GatheringStateCallback gathering_state_callback_ RTC_GUARDED_BY(signal_task_queue_) = nullptr;
    CandidateCallback candidate_callback_ RTC_GUARDED_BY(signal_task_queue_) = nullptr;
    SignalingStateCallback signaling_state_callback_ RTC_GUARDED_BY(signal_task_queue_) = nullptr;

    std::optional<sdp::Description> local_sdp_ RTC_GUARDED_BY(signal_task_queue_) = std::nullopt;
    std::optional<sdp::Description> remote_sdp_ RTC_GUARDED_BY(signal_task_queue_) = std::nullopt;

    std::vector<const sdp::Candidate> remote_candidates_ RTC_GUARDED_BY(signal_task_queue_);

    std::unordered_map<std::string, sdp::Media> media_sdps_ RTC_GUARDED_BY(signal_task_queue_);

    DataChannelCallback data_channel_callback_ RTC_GUARDED_BY(worker_task_queue_) = nullptr;
    MediaTrackCallback media_track_callback_ RTC_GUARDED_BY(worker_task_queue_) = nullptr;

    // Keep a weak reference instead of shared one, since the life cycle of 
    // data channels or media tracks should be owned by the one who has created them.
    std::unordered_map<uint16_t, std::weak_ptr<DataChannel>> data_channels_ RTC_GUARDED_BY(worker_task_queue_);
    std::unordered_map<std::string /* mid */, std::weak_ptr<MediaTrack>> media_tracks_ RTC_GUARDED_BY(worker_task_queue_);

    // The pending data channels will be owned by peer connection before 
    // handled by user, that's why we use shared_ptr here.
    std::vector<std::shared_ptr<DataChannel>> pending_data_channels_ RTC_GUARDED_BY(worker_task_queue_);
    std::vector<std::shared_ptr<MediaTrack>> pending_media_tracks_ RTC_GUARDED_BY(worker_task_queue_);

    RtpDemuxer rtp_demuxer_ RTC_GUARDED_BY(worker_task_queue_);
    
};

RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, PeerConnection::ConnectionState state);
RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, PeerConnection::GatheringState state);
RTC_CPP_EXPORT std::ostream& operator<<(std::ostream& out, PeerConnection::SignalingState state);

} // namespace naivertc

#endif