#ifndef _RTC_ICE_TRANSPORT_H_
#define _RTC_ICE_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/pc/peer_connection_configuration.hpp"
#include "rtc/sdp/candidate.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/sdp/sdp_description.hpp"

#include <sigslot.h>

#if USE_NICE
#include <nice/agent.h>
#include <thread>
#include <chrono>
#else
#include <juice/juice.h>
#endif

namespace naivertc {

class RTC_CPP_EXPORT IceTransport final: public Transport {
public:
    enum class GatheringState {
        NEW = 0,
        GATHERING,
        COMPLETED
    };
public:
    IceTransport(const RtcConfiguration& config);
    ~IceTransport();

    void Stop(StopedCallback callback = nullptr) override;

    sdp::Role role() const;
    void GatherLocalCandidate(std::string mid);
    bool AddRemoteCandidate(const sdp::Candidate& candidate);

    sdp::Description::Builder GetLocalDescription(sdp::Type type) const;
    void SetRemoteDescription(const sdp::Description& remote_sdp);

    std::optional<std::string> GetLocalAddress() const;
    std::optional<std::string> GetRemoteAddress() const;

    std::pair<std::optional<sdp::Candidate>, std::optional<sdp::Candidate>> GetSelectedCandidatePair();

    sigslot::signal1<sdp::Candidate> SignalCandidateGathered;
    sigslot::signal1<GatheringState> SignalGatheringStateChanged;

    void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback = nullptr) override;

private:
    void OnStateChanged(State state);
    void OnGetheringStateChanged(GatheringState state);
    void OnCandidateGathered(const char* sdp);
    void OnDataReceived(const char* data, size_t size);

    sdp::IceSettingPair ParseIceSettingFromSDP(const std::string& sdp) const;
    
    // Override from Transport
    void Outgoing(std::shared_ptr<Packet> out_packet, PacketSentCallback callback = nullptr) override;

private:
#if USE_NICE
    void InitNice(const RtcConfiguration& config);

    std::string NiceAddressToString(const NiceAddress& nice_addr) const;
    void ProcessNiceTimeout();
    void ProcessNiceState(guint state);

    static void OnNiceLog(const gchar* log_domain, GLogLevelFlags log_level, const gchar* message, gpointer user_data);
    static void OnNiceStateChanged(NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer user_data);
    static void OnNiceCandidateGathered(NiceAgent *agent, NiceCandidate *candidate, gpointer user_data);
	static void OnNiceGetheringDone(NiceAgent *agent, guint stream_id, gpointer user_data);
	static void OnNiceDataReceived(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer user_data);
	static gboolean OnNiceTimeout(gpointer user_data);

#else
    void InitJuice(const RtcConfiguration& config);

    static void OnJuiceLog(juice_log_level_t level, const char* message);
    static void OnJuiceStateChanged(juice_agent_t* agent, juice_state_t state, void* user_ptr);
    static void OnJuiceCandidateGathered(juice_agent_t* agent, const char* sdp, void* user_ptr);
    static void OnJuiceGetheringDone(juice_agent_t* agent, void* user_ptr);
    static void OnJuiceDataReceived(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

#endif

private:
#if USE_NICE
    uint32_t stream_id_ = 0;
    const guint component_id_ = 1;
    guint timeout_id_ = 0;
    unsigned int outgoing_dscp_ = 0;
    std::thread main_loop_thread_;
    std::chrono::milliseconds trickle_timeout_;
    std::unique_ptr<NiceAgent, void(*)(gpointer)> nice_agent_{nullptr, nullptr};
    std::unique_ptr<GMainLoop, void(*)(GMainLoop *)> main_loop_{nullptr, nullptr};
#else
    std::unique_ptr<juice_agent_t, void (*)(juice_agent_t *)> juice_agent_{nullptr, nullptr};
#endif
    std::string curr_mid_;
    sdp::Role role_;
    std::atomic<GatheringState> gathering_state_ = GatheringState::NEW;
};

}

#endif