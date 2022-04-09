#ifndef _RTC_TRANSPORTS_ICE_TRANSPORT_H_
#define _RTC_TRANSPORTS_ICE_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/transports/base_transport.hpp"
#include "rtc/pc/peer_connection_configuration.hpp"
#include "rtc/sdp/candidate.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/sdp/sdp_description.hpp"

#if defined(USE_NICE)
#include <nice/agent.h>
#include <thread>
#include <chrono>
#else
#include <juice/juice.h>
#endif

#include <functional>
#include <exception>

namespace naivertc {

class IceTransport final: public BaseTransport {
public:
    // Configuration
    struct Configuration {
        std::vector<IceServer> ice_servers;

        uint16_t port_range_begin = 1024;
        uint16_t port_range_end = 65535;

        bool enable_ice_tcp = false;
        
    #if defined(USE_NICE)
        std::optional<ProxyServer> proxy_server;
    #else
        std::optional<std::string> bind_addresses;
    #endif
    };

    // GatheringState
    enum class GatheringState {
        NEW = 0,
        GATHERING,
        COMPLETED
    };

    // Description
    class Description {
    public:
        static Description Parse(std::string sdp, sdp::Type type = sdp::Type::UNSPEC, sdp::Role role = sdp::Role::ACT_PASS);
    public:
        Description(sdp::Type type = sdp::Type::UNSPEC, 
                    sdp::Role role = sdp::Role::ACT_PASS, 
                    std::optional<std::string> ice_ufrag = std::nullopt, 
                    std::optional<std::string> ice_pwd = std::nullopt);
        ~Description();

        sdp::Type type() const;
        sdp::Role role() const;
        std::optional<std::string> ice_ufrag() const;
        std::optional<std::string> ice_pwd() const;

        std::string GenerateSDP(const std::string eol) const;

    private:
        sdp::Type type_;
        sdp::Role role_;
        std::optional<std::string> ice_ufrag_;
        std::optional<std::string> ice_pwd_;
    };

    using CandidatePair = std::pair<std::optional<sdp::Candidate>, std::optional<sdp::Candidate>>;
    using GatheringStateChangedCallback = std::function<void(GatheringState)>;
    using CandidateGatheredCallback = std::function<void(sdp::Candidate)>;
    using RoleChangedCallback = std::function<void(sdp::Role)>;

public:
    IceTransport(Configuration config, sdp::Role role);
    ~IceTransport() override;

    sdp::Role role() const;
    GatheringState gathering_state() const;
    std::exception_ptr last_exception() const;
    
    bool Start() override;
    bool Stop() override;

    int Send(CopyOnWriteBuffer packet, PacketOptions options) override;

    void StartToGatherLocalCandidate(std::string mid);
    void AddRemoteCandidate(sdp::Candidate candidate);

    Description GetLocalDescription(sdp::Type type) const;
    void SetRemoteDescription(Description remote_sdp);

    std::optional<std::string> GetLocalAddress() const;
    std::optional<std::string> GetRemoteAddress() const;
    CandidatePair GetSelectedCandidatePair() const;

    void OnGatheringStateChanged(GatheringStateChangedCallback callback);
    void OnCandidateGathered(CandidateGatheredCallback callback);
    void OnRoleChanged(RoleChangedCallback callback);

private:
    void NegotiateRole(sdp::Role remote_role);

    void UpdateGatheringState(GatheringState state);
    void OnGatheredCandidate(sdp::Candidate candidate);

    void Incoming(CopyOnWriteBuffer in_packet) override;
    int Outgoing(CopyOnWriteBuffer out_packet, PacketOptions options) override;

private:
#if defined(USE_NICE)
    static std::string ToString(const NiceAddress& nice_addr);

    void InitNice(const Configuration& config);
    void OnNiceTimeout();
    void OnNiceState(guint state);
    void OnNiceGatheringState(GatheringState state);
    void OnNiceGatheredCandidate(sdp::Candidate candidate);
    void OnNiceReceivedData(CopyOnWriteBuffer data);

    static void OnNiceLog(const gchar* log_domain, GLogLevelFlags log_level, const gchar* message, gpointer user_data);
    static void OnNiceStateChanged(NiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer user_data);
    static void OnNiceCandidateGathered(NiceAgent *agent, NiceCandidate *candidate, gpointer user_data);
	static void OnNiceGetheringDone(NiceAgent *agent, guint stream_id, gpointer user_data);
	static void OnNiceDataReceived(NiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer user_data);
	static gboolean OnNiceTimeout(gpointer user_data);

#else
    void InitJuice(const Configuration& config);
    void OnJuiceState(juice_state_t state);
    void OnJuiceGatheringState(GatheringState state);
    void OnJuiceGatheredCandidate(sdp::Candidate candidate);
    void OnJuiceReceivedData(CopyOnWriteBuffer data);

    static void OnJuiceLog(juice_log_level_t level, const char* message);
    static void OnJuiceStateChanged(juice_agent_t* agent, juice_state_t state, void* user_ptr);
    static void OnJuiceCandidateGathered(juice_agent_t* agent, const char* sdp, void* user_ptr);
    static void OnJuiceGetheringDone(juice_agent_t* agent, void* user_ptr);
    static void OnJuiceDataReceived(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

#endif

private:
#if defined(USE_NICE)
    uint32_t stream_id_ = 0;
    const guint component_id_ = 1;
    guint timeout_id_ = 0;
    DSCP last_dscp_ = DSCP::DSCP_DF;
    std::thread main_loop_thread_;
    std::chrono::milliseconds trickle_timeout_;
    std::unique_ptr<NiceAgent, void(*)(gpointer)> nice_agent_{nullptr, nullptr};
    std::unique_ptr<GMainLoop, void(*)(GMainLoop *)> main_loop_{nullptr, nullptr};
#else
    std::unique_ptr<juice_agent_t, void (*)(juice_agent_t *)> juice_agent_{nullptr, nullptr};
#endif

    const Configuration config_;
    std::string curr_mid_;
    sdp::Role role_;
    GatheringState gathering_state_ = GatheringState::NEW;
    CandidateGatheredCallback candidate_gathered_callback_ = nullptr;
    GatheringStateChangedCallback gathering_state_changed_callback_ = nullptr;
    RoleChangedCallback role_changed_callback_ = nullptr;
    
    std::exception_ptr last_exception_;
};

}

#endif