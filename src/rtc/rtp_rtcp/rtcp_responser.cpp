#include "rtc/rtp_rtcp/rtcp_responser.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr int64_t kDefaultExpectedRetransmissionTimeMs = 125;
constexpr size_t kRtcpMaxNackSizeToSend = 253;

RtcpSender::Configuration RtcpConfigurationFromRtpRtcpConfiguration(const RtcpConfiguration& config, 
                                                                    RtcpReceiveFeedbackProvider* rtcp_receive_feedback_provider) {
    RtcpSender::Configuration rtcp_sender_config;
    rtcp_sender_config.audio = config.audio;
    rtcp_sender_config.local_media_ssrc = config.local_media_ssrc;
    rtcp_sender_config.clock = config.clock;
    rtcp_sender_config.rtcp_report_interval_ms = config.rtcp_report_interval_ms;
    rtcp_sender_config.send_transport = config.send_transport;
    rtcp_sender_config.packet_type_counter_observer = config.packet_type_counter_observer;
    rtcp_sender_config.report_block_provider = config.report_block_provider;
    rtcp_sender_config.rtp_send_stats_provider = config.rtp_send_stats_provider;
    rtcp_sender_config.rtcp_receive_feedback_provider = rtcp_receive_feedback_provider;
    return rtcp_sender_config;
}

} // namespace

RtcpResponser::RtcpResponser(const RtcpConfiguration& config)
    : clock_(config.clock),
      rtcp_sender_(RtcpConfigurationFromRtpRtcpConfiguration(config, this)),
      rtcp_receiver_(config),
      nack_last_time_sent_full_ms_(0),
      nack_last_seq_num_sent_(0) {}

RtcpResponser::~RtcpResponser() {}

void RtcpResponser::set_remote_ssrc(uint32_t remote_ssrc) {
    RTC_RUN_ON(&sequence_checker_);
    rtcp_sender_.set_remote_ssrc(remote_ssrc);
    rtcp_receiver_.set_remote_ssrc(remote_ssrc);
}

void RtcpResponser::set_sending(bool enable) {
    RTC_RUN_ON(&sequence_checker_);
    rtcp_sender_.set_sending(enable);
}

RtcpMode RtcpResponser::rtcp_mode() const {
    RTC_RUN_ON(&sequence_checker_);
    return rtcp_sender_.rtcp_mode();
}
    
void RtcpResponser::set_rtcp_mode(RtcpMode mode) {
    RTC_RUN_ON(&sequence_checker_);
    rtcp_sender_.set_rtcp_mode(mode);
}

void RtcpResponser::RegisterPayloadFrequency(int payload_type,
                                             int payload_frequency) {
    RTC_RUN_ON(&sequence_checker_);
    rtcp_sender_.SetRtpClockRate(payload_type, payload_frequency);
 }

TimeDelta RtcpResponser::rtt() const {
    return rtcp_receiver_.rtt();
}

void RtcpResponser::IncomingRtcpPacket(const uint8_t* packet, size_t packet_size) {
    RTC_RUN_ON(&sequence_checker_);
    IncomingRtcpPacket(CopyOnWriteBuffer(packet, packet_size));
}
    
void RtcpResponser::IncomingRtcpPacket(CopyOnWriteBuffer rtcp_packet) {
    RTC_RUN_ON(&sequence_checker_);
    rtcp_receiver_.IncomingRtcpPacket(std::move(rtcp_packet));
}

bool RtcpResponser::SendNack(const std::vector<uint16_t>& nack_list) {
    RTC_RUN_ON(&sequence_checker_);
    if (nack_list.empty()) {
        return false;
    }
    // FIXME: How to make sure that the incoming list 
    // is the same with the last one?
    int64_t now_ms = clock_->now_ms();
    size_t nack_size = nack_list.size();
    size_t offset = 0;
    // Check if it's time to send full NACK list.
    if (TimeToSendFullNackList(now_ms)) {
        nack_last_time_sent_full_ms_ = now_ms;
    } else {
        // Only send extended list.
        if (nack_last_seq_num_sent_ == nack_list.back()) {
            // Last sequence number is the same, don't send list.
            return true;
        }
        for (size_t i = 0; i < nack_list.size(); ++i) {
            if (nack_last_seq_num_sent_ == nack_list[i]) {
                offset = i + 1;
                break;
            }
        }
        nack_size = nack_list.size() - offset;
    }

    if (nack_size > kRtcpMaxNackSizeToSend) {
        nack_size = kRtcpMaxNackSizeToSend;
    }
    nack_last_seq_num_sent_ = nack_list[offset + nack_size - 1];
    rtcp_sender_.SendRtcp(RtcpPacketType::NACK, &nack_list.data()[offset], nack_size);

    return true;
}

std::optional<RttStats> RtcpResponser::GetRttStats(uint32_t ssrc) const {
    RTC_RUN_ON(&sequence_checker_);
    return rtcp_receiver_.GetRttStats(ssrc);
}

bool RtcpResponser::OnReadyToSendRtpFrame(uint32_t timestamp,
                                          int64_t capture_time_ms,
                                          int payload_type,
                                          bool send_sr_before_key_frame) {
    RTC_RUN_ON(&sequence_checker_);
    if (!rtcp_sender_.sending()) {
        return false;
    }

    std::optional<int64_t> capture_time_ms_opt = std::nullopt;
    if (capture_time_ms > 0) {
        capture_time_ms_opt = capture_time_ms;
    }
    std::optional<int> payload_type_opt = std::nullopt;
    if (payload_type >= 0) {
        payload_type_opt = payload_type;
    }
    rtcp_sender_.SetLastRtpTime(timestamp, capture_time_ms_opt, payload_type_opt);
    // Make sure an RTCP report isn't queued behind a key frame.
    if (rtcp_sender_.TimeToSendRtcpReport(send_sr_before_key_frame)) {
        rtcp_sender_.SendRtcp(RtcpPacketType::RTCP_REPORT);
    }
    return true;
}

int64_t RtcpResponser::ExpectedRestransmissionTimeMs() const {
    RTC_RUN_ON(&sequence_checker_);
    auto expected_retransmission_time = rtcp_receiver_.rtt();
    if (expected_retransmission_time.IsFinite()) {
        return expected_retransmission_time.ms();
    }

    // If no RTT available yet, so try to retrieve avg_rtt_ms directly
    // from RTCP receiver.
    auto rtt_stats = rtcp_receiver_.GetRttStats(rtcp_receiver_.remote_ssrc());
    if (rtt_stats) {
        return rtt_stats->avg_rtt().ms();
    }
    return kDefaultExpectedRetransmissionTimeMs;
}

void RtcpResponser::RequestKeyFrame() {
    RTC_RUN_ON(&sequence_checker_);
    // Requests new key frame.
    // using PLI, https://tools.ietf.org/html/rfc4585#section-6.3.1.1
    rtcp_sender_.SendRtcp(RtcpPacketType::PLI);
}

RtcpReceiveFeedback RtcpResponser::GetReceiveFeedback() {
    RTC_RUN_ON(&sequence_checker_);
    RtcpReceiveFeedback receive_feedback;
    receive_feedback.last_sr_stats = rtcp_receiver_.GetLastSenderReportStats();
    receive_feedback.last_xr_rtis = rtcp_receiver_.ConsumeXrDlrrTimeInfos();
    return receive_feedback;
}

// Private methods
bool RtcpResponser::TimeToSendFullNackList(int64_t now_ms) const {
    const int64_t kStartUpRttMs = 100;
    auto rtt_stats = rtcp_receiver_.GetRttStats(rtcp_receiver_.remote_ssrc());

    int64_t wait_time_ms = rtt_stats ? 5 + ((rtt_stats->last_rtt().ms() * 3) >> 1)
                                     : kStartUpRttMs;
    // Send a full NACK list once within every |wait_time_ms|
    return now_ms - nack_last_time_sent_full_ms_ > wait_time_ms;
}
    
} // namespace naivertc
