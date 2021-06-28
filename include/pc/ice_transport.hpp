#ifndef _PC_ICE_TRANSPORT_H_
#define _PC_ICE_TRANSPORT_H_

#include "base/defines.hpp"
#include "pc/transport.hpp"
#include "pc/configuration.hpp"
#include "pc/candidate.hpp"
#include "pc/sdp_defines.hpp"
#include "pc/sdp_session_description.hpp"

#include <sigslot.h>
#include <juice/juice.h>

namespace naivertc {

class RTC_CPP_EXPORT IceTransport: public Transport {
public:
    enum class GatheringState {
        NEW = 0,
        GATHERING,
        COMPLETE
    };
public:
    IceTransport(const Configuration& config);
    ~IceTransport();

    sdp::Role role() const;
    void GatheringLocalCandidate(std::string mid);
    bool AddRemoteCandidate(const Candidate& candidate);

    sdp::SessionDescription GetLocalDescription(sdp::Type type) const;
    void SetRemoteDescription(const sdp::SessionDescription& description);

    std::optional<std::string> GetLocalAddress() const;
    std::optional<std::string> GetRemoteAddress() const;

    sigslot::signal1<Candidate> SignalCandidateGathered;
    sigslot::signal1<GatheringState> SignalGatheringStateChanged;

private:
    void Initialize(const Configuration& config);

    static void OnJuiceLog(juice_log_level_t level, const char* message);
    static void OnJuiceStateChanged(juice_agent_t* agent, juice_state_t state, void* user_ptr);
    static void OnJuiceCandidateGathered(juice_agent_t* agent, const char* sdp, void* user_ptr);
    static void OnJuiceGetheringDone(juice_agent_t* agent, void* user_ptr);
    static void OnJuiceDataReceived(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

    void OnJuiceStateChanged(State state);
    void OnJuiceCandidateGathered(const char* sdp);
    void OnJuiceGetheringStateChanged(GatheringState state);
    void OnJuiceDataReceived(const char* data, size_t size);

private:
    std::unique_ptr<juice_agent_t, void (*)(juice_agent_t *)> juice_agent_;

    std::string curr_mid_;
    sdp::Role role_;
};

}

#endif