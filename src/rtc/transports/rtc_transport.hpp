#ifndef _RTC_TRANSPORTS_RTC_TRANSPORT_H_
#define _RTC_TRANSPORTS_RTC_TRANSPORT_H_

#include "base/defines.hpp"
#include "rtc/base/task_utils/task_queue.hpp"
#include "rtc/transports/ice_transport.hpp"
#include "rtc/transports/dtls_transport.hpp"
#include "rtc/transports/sctp_transport.hpp"
#include "rtc/sdp/candidate.hpp"
#include "rtc/sdp/sdp_description.hpp"

namespace naivertc {

class RTC_CPP_EXPORT RtcTransport {
public:
    struct Configuration {
        // Ice settings
        std::vector<IceServer> ice_servers;

        sdp::Role role = sdp::Role::ACT_PASS;
        bool enable_ice_tcp = false;
        uint16_t port_range_begin = 1024;
        uint16_t port_range_end = 65535;
    #if USE_NICE
        std::optional<ProxyServer> proxy_server;
    #else
        std::optional<std::string> bind_addresses;
    #endif

        // Dtls settings
        std::optional<size_t> mtu;

        // Sctp settings
        uint16_t sctp_port;
        std::optional<size_t> max_message_size;
    };
public:
    RtcTransport(Configuration config, Certificate* certificate, TaskQueue* task_queue);
    ~RtcTransport();

    void Start();
    void Stop();

private:
    void InitIceTransport();
    void InitDtlsTransport();
    void InitSctpTransport();

    // IceTransport callbacks
    void OnIceTransportStateChanged(IceTransport::State transport_state);
    void OnGatheringStateChanged(IceTransport::GatheringState gathering_state);
    void OnCandidateGathered(sdp::Candidate candidate);
    void OnRoleChanged(sdp::Role role);

    // DtlsTransport callbacks
    void OnDtlsTransportStateChanged(DtlsTransport::State transport_state);
    bool OnDtlsVerify(std::string_view fingerprint);
    void OnRtpPacketReceived(CopyOnWriteBuffer in_packet, bool is_rtcp);

private:
    const Configuration config_;
    Certificate* const certificate_;
    TaskQueue* const task_queue_;

    std::unique_ptr<IceTransport> ice_transport_;
    std::unique_ptr<DtlsTransport> dtls_transport_;
    std::unique_ptr<SctpTransport> sctp_transport_;
};
    
} // namespace naivertc


#endif