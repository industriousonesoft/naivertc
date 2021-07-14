#ifndef _PC_ICE_TRANSPORT_H_
#define _PC_ICE_TRANSPORT_H_

#include "base/defines.hpp"
#include "pc/transports/transport.hpp"
#include "pc/peer_connection_configuration.hpp"
#include "pc/sdp/candidate.hpp"
#include "pc/sdp/sdp_defines.hpp"
#include "pc/sdp/sdp_session_description.hpp"

#include <sigslot.h>

#if USE_NICE
#include <nice/agent.h>
#include <thread>
#else
#include <juice/juice.h>
#endif

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
    IceTransport(const RtcConfiguration& config);
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
    
    void OnStateChanged(State state);
    void OnGetheringStateChanged(GatheringState state);
    
    // Override from Transport
    void Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback = nullptr) override;

private:
#if USE_NICE
    void InitNice(const RtcConfiguration& config);

    static void OnNiceLog(const gchar* log_domain, GLogLevelFlags log_level, const gchar* message, gpointer user_data);
#else
    void InitJuice(const RtcConfiguration& config);

    void OnJuiceCandidateGathered(const char* sdp);
    void OnJuiceDataReceived(const char* data, size_t size);

    static void OnJuiceLog(juice_log_level_t level, const char* message);
    static void OnJuiceStateChanged(juice_agent_t* agent, juice_state_t state, void* user_ptr);
    static void OnJuiceCandidateGathered(juice_agent_t* agent, const char* sdp, void* user_ptr);
    static void OnJuiceGetheringDone(juice_agent_t* agent, void* user_ptr);
    static void OnJuiceDataReceived(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

#endif

private:
#if USE_NICE
    uint32_t stream_id_ = 0;
    guint timeout_id_ = 0;
    unsigned int outgoing_dscp_ = 0;
    std::thread main_loop_thread_;
    std::unique_ptr<NiceAgent, void(*)(gpointer)> nice_agent_{nullptr, nullptr};
    std::unique_ptr<GMainLoop, void(*)(GMainLoop *)> main_loop_{nullptr, nullptr};
#else
    std::unique_ptr<juice_agent_t, void (*)(juice_agent_t *)> juice_agent_{nullptr, nullptr};
#endif
    std::string curr_mid_;
    sdp::Role role_;

    std::atomic<GatheringState> gathering_state_ = GatheringState::NONE;
};

}

#endif