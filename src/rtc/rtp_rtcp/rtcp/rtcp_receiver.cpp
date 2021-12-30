#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"

#include <plog/Log.h>

namespace naivertc {

constexpr int kLocalMediaSsrcIndex = 1;
constexpr int kRtxSendSsrcIndex = 2;
constexpr int kFlexFecSsrcIndex = 3;

// RttStats
void RtcpReceiver::RttStats::AddRtt(TimeDelta rtt) {
  last_rtt_ = rtt;
  if (rtt < min_rtt_) {
    min_rtt_ = rtt;
  }
  if (rtt > max_rtt_) {
    max_rtt_ = rtt;
  }
  sum_rtt_ += rtt;
  ++num_rtts_;
}

// RTCPReceiver
RtcpReceiver::RtcpReceiver(const RtcpConfiguration& config, 
                           Observer* const observer) 
    : clock_(config.clock),
      observer_(observer),
      receiver_only_(config.receiver_only),
      remote_ssrc_(0),
      packet_type_counter_observer_(config.packet_type_counter_observer) {
    // Registered ssrcs
    registered_ssrcs_[kLocalMediaSsrcIndex] = config.local_media_ssrc;
    if (config.rtx_send_ssrc.has_value()) {
        registered_ssrcs_[kRtxSendSsrcIndex] = config.rtx_send_ssrc.value();
    }
    if (config.fec_ssrc.has_value()) {
        registered_ssrcs_[kFlexFecSsrcIndex] = config.fec_ssrc.value();
    }
}

RtcpReceiver::~RtcpReceiver() {}

uint32_t RtcpReceiver::local_media_ssrc() const {
    RTC_RUN_ON(&sequence_checker_);
    return registered_ssrcs_.at(kLocalMediaSsrcIndex);
}

uint32_t RtcpReceiver::remote_ssrc() const {
    RTC_RUN_ON(&sequence_checker_);
    return remote_ssrc_;
}

void RtcpReceiver::set_remote_ssrc(uint32_t remote_ssrc) {
    RTC_RUN_ON(&sequence_checker_);
    // New SSRC reset old reports.
    last_received_sr_ntp_.Reset();
    remote_ssrc_ = remote_ssrc;
}

void RtcpReceiver::IncomingPacket(CopyOnWriteBuffer packet) {
    RTC_RUN_ON(&sequence_checker_);
    PacketInfo packet_info;
    if (!ParseCompoundPacket(std::move(packet), &packet_info)) {
        return;
    }
    HandleParseResult(packet_info);
}

bool RtcpReceiver::NTP(uint32_t* received_ntp_secs,
                       uint32_t* received_ntp_frac,
                       uint32_t* rtcp_arrival_time_secs,
                       uint32_t* rtcp_arrival_time_frac,
                       uint32_t* rtcp_timestamp,
                       uint32_t* remote_sender_packet_count,
                       uint64_t* remote_sender_octet_count,
                       uint64_t* remote_sender_reports_count) const {
    RTC_RUN_ON(&sequence_checker_);
    if (!last_received_sr_ntp_.Valid())
        return false;

    // NTP from incoming SenderReport.
    if (received_ntp_secs)
        *received_ntp_secs = remote_sender_ntp_time_.seconds();
    if (received_ntp_frac)
        *received_ntp_frac = remote_sender_ntp_time_.fractions();
    // Rtp time from incoming SenderReport.
    if (rtcp_timestamp)
        *rtcp_timestamp = remote_sender_rtp_time_;

    // Local NTP time when we received a RTCP packet with a send block.
    if (rtcp_arrival_time_secs)
        *rtcp_arrival_time_secs = last_received_sr_ntp_.seconds();
    if (rtcp_arrival_time_frac)
        *rtcp_arrival_time_frac = last_received_sr_ntp_.fractions();

    // Counters.
    if (remote_sender_packet_count)
        *remote_sender_packet_count = remote_sender_packet_count_;
    if (remote_sender_octet_count)
        *remote_sender_octet_count = remote_sender_octet_count_;
    if (remote_sender_reports_count)
        *remote_sender_reports_count = remote_sender_reports_count_;

    return true;
}

int32_t RtcpReceiver::RTT(uint32_t remote_ssrc,
                          int64_t* last_rtt_ms,
                          int64_t* avg_rtt_ms,
                          int64_t* min_rtt_ms,
                          int64_t* max_rtt_ms) const {
    RTC_RUN_ON(&sequence_checker_);
    auto it = rtts_.find(remote_ssrc);
    if (it == rtts_.end()) {
        return -1;
    }
    if (last_rtt_ms) {
        *last_rtt_ms = it->second.last_rtt().ms();
    }

    if (avg_rtt_ms) {
        *avg_rtt_ms = it->second.average_rtt().ms();
    }

    if (min_rtt_ms) {
        *min_rtt_ms = it->second.min_rtt().ms();
    }

    if (max_rtt_ms) {
        *max_rtt_ms = it->second.max_rtt().ms();
    }

    return 0;
}

// Private methods
void RtcpReceiver::HandleParseResult(const PacketInfo& packet_info) {
    // NACK list.
    if (!receiver_only_ && (packet_info.packet_type_flags & rtcp::Nack::kPacketType)) {
        if (!packet_info.nack_list.empty()) {
            PLOG_VERBOSE << "Received NACK list size=" << packet_info.nack_list.size();
            observer_->OnReceivedNack(packet_info.nack_list);
        }
    }

    // REMB
    if (packet_info.packet_type_flags & rtcp::Remb::kPacketType) {
        PLOG_VERBOSE << "Received REMB=" << packet_info.remb_bps << " bps.";
    }

    // Report blocks
    if ((packet_info.packet_type_flags & rtcp::SenderReport::kPacketType) ||
        (packet_info.packet_type_flags & rtcp::ReceiverReport::kPacketType)) {
        // int64_t now_ms = clock_->now_ms();
        PLOG_VERBOSE << "Received report blocks size=" << packet_info.report_block_datas.size();
        observer_->OnReceivedRtcpReportBlocks(packet_info.report_block_datas);
    }

}

bool RtcpReceiver::IsRegisteredSsrc(uint32_t ssrc) const {
    for (const auto& kv : registered_ssrcs_) {
        if (kv.second == ssrc) {
            return true;
        }
    }
    return false;
}

} // namespace naivertc
