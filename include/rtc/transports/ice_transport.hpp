#ifndef _RTC_ICE_TRANSPORT_H_
#define _RTC_ICE_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/transports/transport.hpp"
#include "rtc/pc/peer_connection_configuration.hpp"
#include "rtc/sdp/candidate.hpp"
#include "rtc/sdp/sdp_defines.hpp"
#include "rtc/sdp/sdp_description.hpp"

#if USE_NICE
#include <nice/agent.h>
#include <thread>
#include <chrono>
#else
#include <juice/juice.h>
#endif

#include <functional>
#include <exception>

namespace naivertc {

class RTC_CPP_EXPORT IceTransport final: public Transport {
public:
    enum class GatheringState {
        NEW = 0,
        GATHERING,
        COMPLETED
    };

    using AddressCallback = std::function<void(std::optional<std::string>)>;
    using SelectedCandidatePairCallback = std::function<void(std::pair<std::optional<sdp::Candidate>, std::optional<sdp::Candidate>>)>;

    using GatheringStateChangedCallback = std::function<void(GatheringState)>;
    using CandidateGatheredCallback = std::function<void(sdp::Candidate)>;

    using RoleChangedCallback = std::function<void(sdp::Role)>;

public:
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
public:
    IceTransport(const RtcConfiguration& config, std::shared_ptr<TaskQueue> task_queue = nullptr);
    ~IceTransport();

    sdp::Role role() const;
    std::exception_ptr last_exception() const;

    bool Start() override;
    bool Stop() override;

    void Send(std::shared_ptr<Packet> packet, PacketSentCallback callback) override;
    int Send(std::shared_ptr<Packet> packet) override;

    void StartToGatherLocalCandidate(std::string mid);
    void AddRemoteCandidate(const sdp::Candidate candidate);

    Description GetLocalDescription(sdp::Type type) const;
    void SetRemoteDescription(const Description remote_sdp);

    void GetLocalAddress(AddressCallback callback) const;
    void GetRemoteAddress(AddressCallback callback) const;
    void GetSelectedCandidatePair(SelectedCandidatePairCallback callback) const;

    void OnGatheringStateChanged(GatheringStateChangedCallback callback);
    void OnCandidateGathered(CandidateGatheredCallback callback);
    void OnRoleChanged(RoleChangedCallback callback);

private:
    void NegotiateRole(sdp::Role remote_role);

    void UpdateGatheringState(GatheringState state);
    void ProcessGatheredCandidate(const char* sdp);
    void ProcessReceivedData(const char* data, size_t size);

    void Incoming(std::shared_ptr<Packet> in_packet) override;
    int Outgoing(std::shared_ptr<Packet> out_packet) override;

    int SendInternal(std::shared_ptr<Packet> packet);

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
    CandidateGatheredCallback candidate_gathered_callback_ = nullptr;
    GatheringStateChangedCallback gathering_state_changed_callback_ = nullptr;
    RoleChangedCallback role_changed_callback_ = nullptr;
    
    std::exception_ptr last_exception_;
};

}

#endif