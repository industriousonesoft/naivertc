#include "rtc/rtp_rtcp/rtcp_module.hpp"

namespace naivertc {
namespace {

const int64_t kDefaultExpectedRetransmissionTimeMs = 125;

}  // namespace

void RtcpModule::IncomingPacket(const uint8_t* packet, size_t packet_size) {
    task_queue_->Async([this, packet, packet_size](){
        IncomingPacket(CopyOnWriteBuffer(packet, packet_size));
    });
}
    
void RtcpModule::IncomingPacket(CopyOnWriteBuffer rtcp_packet) {
    task_queue_->Async([this, rtcp_packet=std::move(rtcp_packet)](){
        rtcp_receiver_.IncomingPacket(std::move(rtcp_packet));
    });
}

int32_t RtcpModule::RTT(uint32_t remote_ssrc,
                        int64_t* last_rtt_ms,
                        int64_t* avg_rtt_ms,
                        int64_t* min_rtt_ms,
                        int64_t* max_rtt_ms) const {
    return task_queue_->Sync<int32_t>([&](){
        int ret = rtcp_receiver_.RTT(remote_ssrc, last_rtt_ms, avg_rtt_ms, min_rtt_ms, max_rtt_ms);
        if (last_rtt_ms && *last_rtt_ms == 0) {
            *last_rtt_ms = rtt_ms_;
        }
        return ret;
    });
}

int32_t RtcpModule::RemoteNTP(uint32_t* received_ntp_secs,
                              uint32_t* received_ntp_frac,
                              uint32_t* rtcp_arrival_time_secs,
                              uint32_t* rtcp_arrival_time_frac,
                              uint32_t* rtcp_timestamp) const {
    return task_queue_->Sync<int32_t>([&](){
        return rtcp_receiver_.NTP(received_ntp_secs, 
                                  received_ntp_frac,
                                  rtcp_arrival_time_secs, 
                                  rtcp_arrival_time_frac,
                                  rtcp_timestamp,
                                  /*remote_sender_packet_count=*/nullptr,
                                  /*remote_sender_octet_count=*/nullptr,
                                  /*remote_sender_reports_count=*/nullptr)
                ? 0
                : -1;
    });
}

int64_t RtcpModule::ExpectedRestransmissionTimeMs() const {
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

void RtcpModule::SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set) {

}

void RtcpModule::OnRequestSendReport() {

}

void RtcpModule::OnReceivedNack(const std::vector<uint16_t>& nack_sequence_numbers) {

}

void RtcpModule::OnReceivedRtcpReportBlocks(const ReportBlockList& report_blocks) {

}  
    
} // namespace naivertc
