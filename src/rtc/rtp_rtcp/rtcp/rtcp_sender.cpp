#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr int32_t kDefaultVideoReportIntervalMs = 1000; // 1s
constexpr int32_t kDefaultAudioReportIntervalMs = 5000; // 5s
}  // namespace

RtcpSender::RtcpSender(const RtcpConfiguration& config) 
    : audio_(config.audio),
      local_ssrc_(config.local_media_ssrc),
      remote_ssrc_(config.remote_ssrc),
      clock_(config.clock),
      report_interval_(config.rtcp_report_interval_ms > 0 ? TimeDelta::Millis(config.rtcp_report_interval_ms) 
                                                          : (TimeDelta::Millis(config.audio ? kDefaultAudioReportIntervalMs
                                                                                            : kDefaultVideoReportIntervalMs))),
      max_packet_size_(kIpPacketSize - kTransportOverhead /* Default is UDP/IPv6 */) {
  
    InitBuilders();
}

RtcpSender::~RtcpSender() {}

uint32_t RtcpSender::local_ssrc() const {
    RTC_RUN_ON(&sequence_checker_);
    return local_ssrc_;
}

uint32_t RtcpSender::remote_ssrc() const {
    RTC_RUN_ON(&sequence_checker_);
    return remote_ssrc_;
}

void RtcpSender::set_cname(std::string cname) {
    RTC_RUN_ON(&sequence_checker_);
    assert(cname.size() < kRtcpCNameSize);
    cname_ = cname;
}

void RtcpSender::set_max_rtp_packet_size(size_t max_packet_size) {
    RTC_RUN_ON(&sequence_checker_);
    max_packet_size_ = max_packet_size;
}

void RtcpSender::set_csrcs(std::vector<uint32_t> csrcs) {
    RTC_RUN_ON(&sequence_checker_);
    csrcs_ = std::move(csrcs);
}

void RtcpSender::SetRtpClockRate(int8_t rtp_payload_type, int rtp_clock_rate_hz) {
    RTC_RUN_ON(&sequence_checker_);
    rtp_clock_rates_khz_[rtp_payload_type] = rtp_clock_rate_hz / 1000;
}

void RtcpSender::SetTimestampOffset(uint32_t timestamp_offset) {
    RTC_RUN_ON(&sequence_checker_);
    timestamp_offset_ = timestamp_offset;
}

void RtcpSender::SetLastRtpTime(uint32_t rtp_timestamp,
                                std::optional<Timestamp> capture_time,
                                std::optional<int8_t> rtp_payload_type) {
    RTC_RUN_ON(&sequence_checker_);
    if (rtp_payload_type.has_value()) {
        last_rtp_payload_type_ = rtp_payload_type.value();
    }
    last_rtp_timestamp_ = rtp_timestamp;
    if (!capture_time.has_value()) {
        last_frame_capture_time_ = clock_->CurrentTime();
    } else {
        last_frame_capture_time_ = capture_time;
    }
}

bool RtcpSender::Sending() const {
    RTC_RUN_ON(&sequence_checker_);
    return sending_;
}
    
void RtcpSender::SetSendingStatus(const FeedbackState& feedback_state, bool enable) {
    RTC_RUN_ON(&sequence_checker_);
    bool send_rtcp_bye = false;
    if (enable == false && sending_ == true) {
        send_rtcp_bye = true;
    }
    sending_ = enable;
    if (send_rtcp_bye) {
        // TODO: send RTCP bye packet
    }
}

void RtcpSender::SetRemb(uint64_t bitrate_bps, std::vector<uint32_t> ssrcs) {
    RTC_RUN_ON(&sequence_checker_);
    remb_bitrate_ = bitrate_bps;
    remb_ssrcs_ = std::move(ssrcs);

    SetFlag(RtcpPacketType::REMB, false);
    // Send a REMB immediately if we have a new REMB. The frequency of REMBs is
    // throttled by the caller.
    SetNextRtcpSendEvaluationDuration(TimeDelta::Zero());
}

bool RtcpSender::TimeToSendRtcpReport(bool send_rtcp_before_key_frame) {
    RTC_RUN_ON(&sequence_checker_);
    // RTCP Transmission Interval: https://datatracker.ietf.org/doc/html/rfc3550#section-6.2
    Timestamp now = clock_->CurrentTime();
    if (!audio_ && send_rtcp_before_key_frame) {
        // for video key-frames we want to send the RTCP before the large key-frame
        // if we have a 100 ms margin
        now += TimeDelta::Millis(100);
    }
    return now >= next_time_to_send_rtcp_.value();
}

bool RtcpSender::SendRtcp(const FeedbackState& feedback_state,
                          RtcpPacketType packet_type,
                          const std::vector<uint16_t> nackList) {
    RTC_RUN_ON(&sequence_checker_);
    bool bRet = false;
    auto callback = [&](BinaryBuffer packet) {
        // TODO: Send RTCP packet by transport
    };
    PacketSender sender(callback, max_packet_size_);
    bRet = ComputeCompoundRtcpPacket(feedback_state, packet_type, std::move(nackList), sender);
    if (bRet) {
        sender.Send();
    }
    return bRet;
}

bool RtcpSender::SendLossNotification(const FeedbackState& feedback_state,
                                      uint16_t last_decoded_seq_num,
                                      uint16_t last_received_seq_num,
                                      bool decodability_flag,
                                      bool buffering_allowed) {
    RTC_RUN_ON(&sequence_checker_);
    bool bRet = false;
    auto callback = [&](BinaryBuffer packet) {
        // TODO: Send RTCP packet by transport
    };
    
    if (!loss_notification_.Set(last_decoded_seq_num, last_received_seq_num, decodability_flag)) {
        return false;
    }

    SetFlag(RtcpPacketType::LOSS_NOTIFICATION, true);

    // The loss notification will be batched with additional feedback messages.
    if (buffering_allowed) {
        return true;
    }

    PacketSender sender(callback, max_packet_size_);
    bRet = ComputeCompoundRtcpPacket(feedback_state, RtcpPacketType::LOSS_NOTIFICATION, {}, sender);
    if (bRet) {
        sender.Send();
    }
    return bRet;
}

void RtcpSender::OnNextSendEvaluationTimeScheduled(NextSendEvaluationTimeScheduledCallback callback) {
    RTC_RUN_ON(&sequence_checker_);
    next_send_evaluation_time_scheduled_callback_ = callback;
}

// Private methods
void RtcpSender::SetNextRtcpSendEvaluationDuration(TimeDelta duration) {
    next_time_to_send_rtcp_ = clock_->CurrentTime() + duration;
    if (next_send_evaluation_time_scheduled_callback_) {
        next_send_evaluation_time_scheduled_callback_(duration);
    }
}

void RtcpSender::SetFlag(RtcpPacketType type, bool is_volatile) {
    report_flags_.insert(ReportFlag(type, is_volatile));
}

bool RtcpSender::IsFlagPresent(RtcpPacketType type) const {
    return report_flags_.find(ReportFlag(type, false)) != report_flags_.end();
}

bool RtcpSender::ConsumeFlag(RtcpPacketType type, bool forced) {
    auto it = report_flags_.find(ReportFlag(type, false));
    if (it == report_flags_.end())
        return false;
    if (it->is_volatile || forced)
        report_flags_.erase((it));
    return true;
}

bool RtcpSender::AllVolatileFlagsConsumed() const {
    for (const ReportFlag& flag : report_flags_) {
        if (flag.is_volatile)
        return false;
    }
    return true;
}
 
} // namespace naivertc
