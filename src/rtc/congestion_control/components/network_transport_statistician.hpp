#ifndef _RTC_CONGESION_CONTROLLER_COMPONENTS_NETWORK_TRANSPORT_STATISTICIAN_H_
#define _RTC_CONGESION_CONTROLLER_COMPONENTS_NETWORK_TRANSPORT_STATISTICIAN_H_

#include "base/defines.hpp"
#include "rtc/base/synchronization/sequence_checker.hpp"
#include "rtc/congestion_control/base/bwe_types.hpp"
#include "rtc/rtp_rtcp/base/rtp_statistic_types.hpp"
#include "rtc/rtp_rtcp/components/num_unwrapper.hpp"

#include <map>
#include <optional>

namespace naivertc {

namespace rtcp {
class TransportFeedback;
}

// NetworkTransportStatistician
class NetworkTransportStatistician {
public:
    NetworkTransportStatistician();
    ~NetworkTransportStatistician();

    size_t GetInFlightBytes() const;

    void AddPacket(const RtpPacketSendInfo& packet_info, 
                   size_t overhead_bytes, 
                   Timestamp receive_time);

    std::optional<SentPacket> ProcessSentPacket(const RtpSentPacket& sent_packet);
    std::optional<TransportPacketsFeedback> ProcessTransportFeedback(const rtcp::TransportFeedback& feedback,
                                                                     Timestamp receive_time);
private:
    bool IsInFlight(const SentPacket& packet);

    bool ParsePacketFeedbacks(const rtcp::TransportFeedback& feedback,
                              Timestamp receive_time,
                              TransportPacketsFeedback& report);

private:
    // PacketFeedback
    struct PacketFeedback {
        SentPacket sent;
        // Time corresponding to when this object was created.
        Timestamp creation_time = Timestamp::MinusInfinity();
        // Time corresponding to when the packet associated with |sent| was received. 
        Timestamp receive_time = Timestamp::PlusInfinity();
    };

private:
    SequenceChecker sequence_checker_;
    std::map<int64_t, PacketFeedback> packet_fb_history_;

    SeqNumUnwrapper seq_num_unwrapper_;

    int64_t last_acked_packet_id_;
    size_t inflight_bytes_;

    Timestamp last_send_time_;
    Timestamp last_untracked_send_time_;
    size_t pending_untracked_bytes_;

    Timestamp last_feedback_recv_time_;
    TimeDelta last_timestamp_;
};
    
} // namespace naivertc


#endif