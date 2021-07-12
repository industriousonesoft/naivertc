#ifndef _PC_ICE_TRANSPORT_H_
#define _PC_ICE_TRANSPORT_H_

#include "base/defines.hpp"
#include "pc/transports/transport.hpp"
#include "pc/peer_connection_configuration.hpp"
#include "pc/sdp/candidate.hpp"
#include "pc/sdp/sdp_defines.hpp"
#include "pc/sdp/sdp_session_description.hpp"

#include <sigslot.h>
#include <juice/juice.h>

namespace naivertc {

class RTC_CPP_EXPORT IceTransport final: public Transport {
public:
    enum class GatheringState {
        NONE = -1,
        NEW = 0,
        GATHERING,
        COMPLETED
    };
public:
    IceTransport(const Configuration& config);
    ~IceTransport();

    sdp::Role role() const;
    void GatheringLocalCandidate(std::string mid);
    bool AddRemoteCandidate(const Candidate& candidate);

    sdp::SessionDescription GetLocalDescription(sdp::Type type) const;
    void SetRemoteDescription(const sdp::SessionDescription& remote_sdp);

    std::optional<std::string> GetLocalAddress() const;
    std::optional<std::string> GetRemoteAddress() const;

    sigslot::signal1<Candidate> SignalCandidateGathered;
    sigslot::signal1<GatheringState> SignalGatheringStateChanged;

    void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback = nullptr) override;

private:
    void InitJuice(const Configuration& config);

    static void OnJuiceLog(juice_log_level_t level, const char* message);
    static void OnJuiceStateChanged(juice_agent_t* agent, juice_state_t state, void* user_ptr);
    static void OnJuiceCandidateGathered(juice_agent_t* agent, const char* sdp, void* user_ptr);
    static void OnJuiceGetheringDone(juice_agent_t* agent, void* user_ptr);
    static void OnJuiceDataReceived(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

    void OnJuiceStateChanged(State state);
    void OnJuiceCandidateGathered(const char* sdp);
    void OnJuiceGetheringStateChanged(GatheringState state);
    void OnJuiceDataReceived(const char* data, size_t size);

    // Override from Transport
    void Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback = nullptr) override;

private:
    std::unique_ptr<juice_agent_t, void (*)(juice_agent_t *)> juice_agent_;

    std::string curr_mid_;
    sdp::Role role_;

    std::atomic<GatheringState> gathering_state_ = GatheringState::NONE;
};

}

#endif