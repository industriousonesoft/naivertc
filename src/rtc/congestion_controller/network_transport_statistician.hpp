#ifndef _RTC_CALL_RTP_TRANSPORT_STATISTICIAN_H_
#define _RTC_CALL_RTP_TRANSPORT_STATISTICIAN_H_

#include "base/defines.hpp"
#include "rtc/congestion_controller/network_types.hpp"
#include "rtc/rtp_rtcp/base/rtp_statistic_structs.hpp"
#include "rtc/rtp_rtcp/components/num_unwrapper.hpp"

#include <map>

namespace naivertc {

class Clock;

class RTC_CPP_EXPORT NetworkTransportStatistician {
public:
    NetworkTransportStatistician(Clock* clock);
    ~NetworkTransportStatistician();

    void OnSendFeedback(const RtpSendFeedback& send_feedback, size_t overhead_bytes);


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
    bool IsInFlight(const SentPacket& packet);

private:
    Clock* const clock_;
    std::map<uint64_t, PacketFeedback> packet_fb_history_;

    SeqNumUnwrapper seq_num_unwrapper_;

    int64_t last_acked_packet_id_;
    size_t inflight_bytes_;
};
    
} // namespace naivertc


#endif