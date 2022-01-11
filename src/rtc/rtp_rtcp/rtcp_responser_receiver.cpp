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

int32_t RtcpResponser::RTT(uint32_t remote_ssrc,
                        int64_t* last_rtt_ms,
                        int64_t* avg_rtt_ms,
                        int64_t* min_rtt_ms,
                        int64_t* max_rtt_ms) const {
    RTC_RUN_ON(&sequence_checker_);
    int ret = rtcp_receiver_.RTT(remote_ssrc, last_rtt_ms, avg_rtt_ms, min_rtt_ms, max_rtt_ms);
    if (last_rtt_ms && *last_rtt_ms == 0) {
        *last_rtt_ms = rtt_ms_;
    }
    return ret;
}

int32_t RtcpResponser::RemoteNTP(uint32_t* received_ntp_secs,
                              uint32_t* received_ntp_frac,
                              uint32_t* rtcp_arrival_time_secs,
                              uint32_t* rtcp_arrival_time_frac,
                              uint32_t* rtcp_timestamp) const {
    RTC_RUN_ON(&sequence_checker_);
    bool bRet = rtcp_receiver_.NTP(received_ntp_secs, 
                                   received_ntp_frac,
                                   rtcp_arrival_time_secs, 
                                   rtcp_arrival_time_frac,
                                   rtcp_timestamp,
                                   /*remote_sender_packet_count=*/nullptr,
                                   /*remote_sender_octet_count=*/nullptr,
                                   /*remote_sender_reports_count=*/nullptr);
    return bRet ? 0 : -1;
}

int64_t RtcpResponser::ExpectedRestransmissionTimeMs() const {
    RTC_RUN_ON(&sequence_checker_);
    int64_t expected_retransmission_time_ms = rtt_ms_;
    if (expected_retransmission_time_ms > 0) {
        return expected_retransmission_time_ms;
    }

    // If no RTT available yet, so try to retrieve avg_rtt_ms directly
    // from RTCP receiver.
    if (rtcp_receiver_.RTT(/*remote_ssrc=*/rtcp_receiver_.remote_ssrc(),
                           /*last_rtt_ms=*/nullptr,
                           /*avg_rtt_ms=*/&expected_retransmission_time_ms,
                           /*min_rtt_ms=*/nullptr,
                           /*max_rtt_ms=*/nullptr) == 0) {
        return expected_retransmission_time_ms;
    }
    return kDefaultExpectedRetransmissionTimeMs;
}

void RtcpResponser::OnRequestSendReport() {
    RTC_RUN_ON(&sequence_checker_);
}

void RtcpResponser::OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers) {
    RTC_RUN_ON(&sequence_checker_);
}

void RtcpResponser::OnReceivedRtcpReportBlocks(const std::vector<RtcpReportBlock>& report_blocks) {
    RTC_RUN_ON(&sequence_checker_);
}  
    
} // namespace naivertc
