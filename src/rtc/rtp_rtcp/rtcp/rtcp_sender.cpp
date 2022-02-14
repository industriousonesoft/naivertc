#include "rtc/rtp_rtcp/rtcp/rtcp_sender.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
constexpr int32_t kDefaultVideoReportIntervalMs = 1000; // 1s
constexpr int32_t kDefaultAudioReportIntervalMs = 5000; // 5s
}  // namespace

RtcpSender::RtcpSender(Configuration config) 
    : audio_(config.audio),
      local_ssrc_(config.local_media_ssrc),
      remote_ssrc_(0),
      clock_(config.clock),
      rtcp_mode_(RtcpMode::OFF),
      report_interval_(config.rtcp_report_interval_ms > 0 ? TimeDelta::Millis(config.rtcp_report_interval_ms) 
                                                          : (TimeDelta::Millis(audio_ ? kDefaultAudioReportIntervalMs
                                                                                      : kDefaultVideoReportIntervalMs))),
      sending_(false),
      sequence_number_fir_(0),
      packet_sender_(config.send_transport, audio_, kIpPacketSize - kTransportOverhead /* Default is UDP/IPv6 */),
      packet_type_counter_observer_(config.packet_type_counter_observer),
      report_block_provider_(config.report_block_provider),
      rtp_send_stats_provider_(config.rtp_send_stats_provider),
      rtcp_receive_feedback_provider_(config.rtcp_receive_feedback_provider),
      work_queue_(TaskQueueImpl::Current()) {
  
    // Initialize all builders.
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

void RtcpSender::set_remote_ssrc(uint32_t remote_ssrc) {
    RTC_RUN_ON(&sequence_checker_);
    remote_ssrc_ = remote_ssrc;
}

void RtcpSender::set_cname(std::string cname) {
    RTC_RUN_ON(&sequence_checker_);
    assert(cname.size() < kRtcpCNameSize);
    cname_ = cname;
}

void RtcpSender::set_max_rtp_packet_size(size_t max_packet_size) {
    RTC_RUN_ON(&sequence_checker_);
    packet_sender_.set_max_packet_size(max_packet_size);
}

void RtcpSender::set_csrcs(std::vector<uint32_t> csrcs) {
    RTC_RUN_ON(&sequence_checker_);
    csrcs_ = std::move(csrcs);
}

void RtcpSender::SetRtpClockRate(int8_t rtp_payload_type, int rtp_clock_rate_hz) {
    RTC_RUN_ON(&sequence_checker_);
    rtp_clock_rates_khz_[rtp_payload_type] = rtp_clock_rate_hz / 1000;
}

void RtcpSender::SetLastRtpTime(uint32_t rtp_timestamp,
                                std::optional<int64_t> capture_time_ms,
                                std::optional<int8_t> rtp_payload_type) {
    RTC_RUN_ON(&sequence_checker_);
    if (rtp_payload_type.has_value()) {
        last_rtp_payload_type_ = rtp_payload_type.value();
    }
    last_rtp_timestamp_ = rtp_timestamp;
    if (!capture_time_ms.has_value()) {
        last_frame_capture_time_ms_ = clock_->now_ms();
    } else {
        last_frame_capture_time_ms_ = capture_time_ms;
    }
}

bool RtcpSender::sending() const {
    RTC_RUN_ON(&sequence_checker_);
    return sending_;
}
    
void RtcpSender::set_sending(bool enable) {
    RTC_RUN_ON(&sequence_checker_);
    bool send_rtcp_bye = false;
    if (rtcp_mode_ != RtcpMode::OFF) {
        if (enable == false && sending_ == true) {
            // Trigger RTCP byte.
            send_rtcp_bye = true;
        }
    }
    sending_ = enable;
    if (send_rtcp_bye) {
        if (!SendRtcp(RtcpPacketType::BYE)) {
            PLOG_WARNING << "Failed to send RTCP bye.";
        }
    }
}

RtcpMode RtcpSender::rtcp_mode() const {
    RTC_RUN_ON(&sequence_checker_);
    return rtcp_mode_;
}
    
void RtcpSender::set_rtcp_mode(RtcpMode mode) {
    if (mode == RtcpMode::OFF) {
        next_time_to_send_rtcp_ = std::nullopt;
    } else if (rtcp_mode_ == RtcpMode::OFF) {
        // When the RTCP mode switchs on, reschdule the next packet.
        ScheduleForNextRtcpSend(report_interval_ / 2);
    }
    rtcp_mode_ = mode;
}

void RtcpSender::SetRemb(uint64_t bitrate_bps, std::vector<uint32_t> ssrcs) {
    RTC_RUN_ON(&sequence_checker_);
    remb_bitrate_ = bitrate_bps;
    remb_ssrcs_ = std::move(ssrcs);

    SetFlag(RtcpPacketType::REMB, false);
    // Send a REMB immediately if we have a new REMB. The frequency of REMBs is
    // throttled by the caller.
    ScheduleForNextRtcpSend(TimeDelta::Zero());
}

void RtcpSender::UnsetRemb() {
    RTC_RUN_ON(&sequence_checker_);
    ConsumeFlag(RtcpPacketType::REMB, /*forced=*/true);
}

bool RtcpSender::TimeToSendRtcpReport(bool send_rtcp_before_key_frame) {
    RTC_RUN_ON(&sequence_checker_);
    if (rtcp_mode_ == RtcpMode::OFF) {
        return false;
    }
    // RTCP Transmission Interval: 
    // For audio we use a configurable interval (default: 5 seconds).
    // For video we use a configurable interval (default: 1 second) for a BW
    // smaller than 360 kbit/s, technicaly we break the max 5% RTCP BW for
    // video below 10 kbit/s but that should be extremely rare.
    // See https://datatracker.ietf.org/doc/html/rfc3550#section-6.2
    Timestamp now = clock_->CurrentTime();
    if (!audio_ && send_rtcp_before_key_frame) {
        // for video key-frames we want to send the RTCP before the large key-frame
        // if we have a 100 ms margin
        now += TimeDelta::Millis(100);
    }
    return now >= next_time_to_send_rtcp_.value();
}

bool RtcpSender::SendRtcp(RtcpPacketType packet_type,
                          const uint16_t* nack_list,
                          size_t nack_size) {
    RTC_RUN_ON(&sequence_checker_);
    packet_sender_.Reset();
    if (!BuildCompoundRtcpPacket(packet_type, 
                                 nack_list, 
                                 nack_size, 
                                 packet_sender_)) {
        return false;
    }
    packet_sender_.Send();
    return true;
}

bool RtcpSender::SendLossNotification(uint16_t last_decoded_seq_num,
                                      uint16_t last_received_seq_num,
                                      bool decodability_flag,
                                      bool buffering_allowed) {
    RTC_RUN_ON(&sequence_checker_);
    if (!loss_notification_.Set(last_decoded_seq_num, last_received_seq_num, decodability_flag)) {
        return false;
    }

    SetFlag(RtcpPacketType::LOSS_NOTIFICATION, true);

    // The loss notification will be batched with additional feedback messages.
    if (buffering_allowed) {
        return true;
    }

    packet_sender_.Reset();
    if (!BuildCompoundRtcpPacket(RtcpPacketType::LOSS_NOTIFICATION, 
                                 /*nack_list*/nullptr, 
                                 /*nack_size*/0, 
                                 packet_sender_)) {
        return false;
    }
    packet_sender_.Send();
    return true;
}

// Private methods
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
    // The volatile packet will be removed after consumed.
    if (it->is_volatile || forced)
        report_flags_.erase((it));
    return true;
}

bool RtcpSender::AllVolatileFlagsConsumed() const {
    for (const ReportFlag& flag : report_flags_) {
        if (flag.is_volatile) {
            return false;
        }
    }
    return true;
}
 
} // namespace naivertc
