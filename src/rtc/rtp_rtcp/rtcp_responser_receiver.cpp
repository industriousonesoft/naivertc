#include "rtc/rtp_rtcp/rtcp_responser.hpp"

namespace naivertc {
namespace {

const int64_t kDefaultExpectedRetransmissionTimeMs = 125;

}  // namespace

void RtcpResponser::IncomingPacket(const uint8_t* packet, size_t packet_size) {
    RTC_RUN_ON(&sequence_checker_);
    IncomingPacket(CopyOnWriteBuffer(packet, packet_size));
}
    
void RtcpResponser::IncomingPacket(CopyOnWriteBuffer rtcp_packet) {
    RTC_RUN_ON(&sequence_checker_);
    rtcp_receiver_.IncomingPacket(std::move(rtcp_packet));
}

std::optional<RttStats> RtcpResponser::GetRttStats(uint32_t ssrc) const {
    RTC_RUN_ON(&sequence_checker_);
    return rtcp_receiver_.GetRttStats(ssrc);
}

int64_t RtcpResponser::ExpectedRestransmissionTimeMs() const {
    RTC_RUN_ON(&sequence_checker_);
    int64_t expected_retransmission_time_ms = rtt_ms_;
    if (expected_retransmission_time_ms > 0) {
        return expected_retransmission_time_ms;
    }

    // If no RTT available yet, so try to retrieve avg_rtt_ms directly
    // from RTCP receiver.
    auto rtt_stats = rtcp_receiver_.GetRttStats(rtcp_receiver_.remote_ssrc());
    if (rtt_stats) {
        return rtt_stats->avg_rtt_ms();
    }
    return kDefaultExpectedRetransmissionTimeMs;
}

RtcpReceiveFeedback RtcpResponser::GetReceiveFeedback() {
    RTC_RUN_ON(&sequence_checker_);
    RtcpReceiveFeedback receive_feedback;
    receive_feedback.last_sr_stats = rtcp_receiver_.GetLastSenderReportStats();;
    return receive_feedback;
}

} // namespace naivertc
