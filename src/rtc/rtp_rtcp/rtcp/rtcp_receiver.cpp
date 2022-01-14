#include "rtc/rtp_rtcp/rtcp/rtcp_receiver.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {

constexpr int kLocalMediaSsrcIndex = 1;
constexpr int kRtxSendSsrcIndex = 2;
constexpr int kFlexFecSsrcIndex = 3;

// The number of RTCP time intervals needed to trigger a timeout.
constexpr int kRrTimeoutIntervals = 3;

constexpr TimeDelta kDefaultVideoReportInterval = TimeDelta::Seconds(1);
constexpr TimeDelta kDefaultAudioReportInterval = TimeDelta::Seconds(5);

constexpr TimeDelta kRttUpdateInterval = TimeDelta::Millis(1000); // 1s

// Returns true if the |timestamp| has exceeded the |interval *
// kRrTimeoutIntervals| period and was reset (set to PlusInfinity()). Returns
// false if the timer was either already reset or if it has not expired.
bool ResetTimestampIfExpired(const Timestamp now,
                             Timestamp& timestamp,
                             TimeDelta interval) {
    if (timestamp.IsInfinite() ||
        now <= timestamp + interval * kRrTimeoutIntervals) {
        return false;
    }

    timestamp = Timestamp::PlusInfinity();
    return true;
}
    
} // namespace

// RTCPReceiver
RtcpReceiver::RtcpReceiver(const RtcpConfiguration& config) 
    : clock_(config.clock),
      receiver_only_(config.receiver_only),
      remote_ssrc_(),
      report_interval_(config.rtcp_report_interval_ms > 0
                           ? TimeDelta::Millis(config.rtcp_report_interval_ms)
                           : (config.audio ? kDefaultAudioReportInterval
                                           : kDefaultVideoReportInterval)),
      rtt_(TimeDelta::PlusInfinity()),
      last_time_received_rb_(Timestamp::PlusInfinity()),
      last_time_increased_sequence_number_(Timestamp::PlusInfinity()),
      num_skipped_packets_(0),
      last_skipped_packets_warning_ms_(clock_->now_ms()),
      work_queue_(TaskQueueImpl::Current()),
      packet_type_counter_observer_(config.packet_type_counter_observer),
      intra_frame_observer_(config.intra_frame_observer),
      loss_notification_observer_(config.loss_notification_observer),
      bandwidth_observer_(config.bandwidth_observer),
      cname_observer_(config.cname_observer),
      rtt_observer_(config.rtt_observer),
      transport_feedback_observer_(config.transport_feedback_observer),
      nack_list_observer_(config.nack_list_observer),
      report_blocks_observer_(config.report_blocks_observer) {

    // Registered ssrcs
    registered_ssrcs_[kLocalMediaSsrcIndex] = config.local_media_ssrc;
    if (config.rtx_send_ssrc.has_value()) {
        registered_ssrcs_[kRtxSendSsrcIndex] = config.rtx_send_ssrc.value();
    }
    if (config.fec_ssrc.has_value()) {
        registered_ssrcs_[kFlexFecSsrcIndex] = config.fec_ssrc.value();
    }

#if !ENABLE_TESTS
    // RTT update repeated task.
    rtt_update_task_ = RepeatingTask::DelayedStart(clock_, work_queue_, kRttUpdateInterval, [this](){
        RttPeriodicUpdate();
        return kRttUpdateInterval;
    });
#endif
}

RtcpReceiver::~RtcpReceiver() {
    RTC_RUN_ON(&sequence_checker_);
#if !ENABLE_TESTS
    rtt_update_task_->Stop();
    rtt_update_task_.reset();
#endif
}

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
    last_sr_stats_.arrival_ntp_time.Reset();
    remote_ssrc_ = remote_ssrc;
}

TimeDelta RtcpReceiver::rtt() const {
    RTC_RUN_ON(&sequence_checker_);
    return rtt_;
}

void RtcpReceiver::IncomingPacket(CopyOnWriteBuffer packet) {
    RTC_RUN_ON(&sequence_checker_);
    PacketInfo packet_info;
    if (!ParseCompoundPacket(std::move(packet), &packet_info)) {
        return;
    }
    HandleParseResult(packet_info);
}

std::optional<RtcpSenderReportStats> RtcpReceiver::GetLastSenderReportStats() const {
    RTC_RUN_ON(&sequence_checker_);
    if (!last_sr_stats_.arrival_ntp_time.Valid()) {
        return std::nullopt;
    }
    return last_sr_stats_;
}

std::optional<RttStats> RtcpReceiver::GetRttStats(uint32_t ssrc) const {
    RTC_RUN_ON(&sequence_checker_);
    auto it = rtts_.find(ssrc);
    if (it == rtts_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<RtcpReportBlock> RtcpReceiver::GetLatestReportBlocks() const {
    RTC_RUN_ON(&sequence_checker_);
    std::vector<RtcpReportBlock> result;
    for (const auto& report : received_report_blocks_) {
        result.push_back(report.second);
    }
    return result;
}

int64_t RtcpReceiver::LastReceivedReportBlockMs() const {
    return last_time_received_rb_.IsFinite() ? last_time_received_rb_.ms() : 0;
}

bool RtcpReceiver::RtcpRrTimeout() {
    RTC_RUN_ON(&sequence_checker_);
    return RtcpRrTimeoutLocked(clock_->CurrentTime());
}

bool RtcpReceiver::RtcpRrSequenceNumberTimeout() {
    RTC_RUN_ON(&sequence_checker_);
    return RtcpRrSequenceNumberTimeoutLocked(clock_->CurrentTime());
}

// Private methods
void RtcpReceiver::HandleParseResult(const PacketInfo& packet_info) {
    RTC_RUN_ON(&sequence_checker_);
    // NACK list.
    if (nack_list_observer_ && !receiver_only_ && (packet_info.packet_type_flags & RtcpPacketType::NACK)) {
        if (!packet_info.nack_list.empty()) {
            PLOG_VERBOSE << "Received NACK list size=" << packet_info.nack_list.size();
            int64_t avg_rtt_ms = 0;
            auto rtt_stats = GetRttStats(remote_ssrc_);
            if (rtt_stats) {
                avg_rtt_ms = rtt_stats->avg_rtt().ms();
            }
            nack_list_observer_->OnReceivedNack(packet_info.nack_list, avg_rtt_ms);
        }
    }

    // Intra frame
    if (intra_frame_observer_ && 
        (packet_info.packet_type_flags & RtcpPacketType::PLI ||
         packet_info.packet_type_flags & RtcpPacketType::FIR)) {
        intra_frame_observer_->OnReceivedIntraFrameRequest(local_media_ssrc());
    }

    // REMB
    if (bandwidth_observer_ && packet_info.packet_type_flags & RtcpPacketType::REMB) {
        PLOG_VERBOSE << "Received REMB=" 
                     << packet_info.remb_bps 
                     << " bps.";
        bandwidth_observer_->OnReceivedEstimatedBitrateBps(packet_info.remb_bps);
    }

    // Report blocks
    if (report_blocks_observer_) {
        if ((packet_info.packet_type_flags & RtcpPacketType::SR) ||
            (packet_info.packet_type_flags & RtcpPacketType::RR)) {
            PLOG_VERBOSE << "Received report blocks size=" 
                         << packet_info.report_blocks.size();
            report_blocks_observer_->OnReceivedRtcpReportBlocks(packet_info.report_blocks, packet_info.rtt_ms);
        }
    }
}

bool RtcpReceiver::IsRegisteredSsrc(uint32_t ssrc) const {
    RTC_RUN_ON(&sequence_checker_);
    for (const auto& kv : registered_ssrcs_) {
        if (kv.second == ssrc) {
            return true;
        }
    }
    return false;
}

void RtcpReceiver::RttPeriodicUpdate() {
    RTC_RUN_ON(&sequence_checker_);
    std::optional<TimeDelta> curr_rtt = std::nullopt;
    if (!receiver_only_) {
        Timestamp time_expired = clock_->CurrentTime() + kRttUpdateInterval;
        if (last_time_received_rb_.IsFinite() && last_time_received_rb_ > time_expired) {
            TimeDelta max_rtt = TimeDelta::MinusInfinity();
            for (const auto& rtt_stats : rtts_) {
                if (rtt_stats.second.last_rtt() > max_rtt) {
                    max_rtt = rtt_stats.second.last_rtt();
                }
            }
            if (max_rtt.IsFinite()) {
                curr_rtt = max_rtt;
            }
        }

        if (RtcpRrTimeout()) {
            PLOG_WARNING << "Timeout: No RTCP RR received.";
        } else if (RtcpRrSequenceNumberTimeout()) {
            PLOG_WARNING << "Timeout: No increase in RTCP RR extended highest sequence number.";
        }
    } else {
        // TODO: Report RTT from receiver.
    }

    if (curr_rtt) {
        if (rtt_observer_) {
            rtt_observer_->OnRttUpdated(*curr_rtt);
        }
        rtt_ = *curr_rtt;
    }
}

bool RtcpReceiver::RtcpRrTimeoutLocked(Timestamp now) {
    RTC_RUN_ON(&sequence_checker_);
    return ResetTimestampIfExpired(now, last_time_received_rb_, report_interval_);
}

bool RtcpReceiver::RtcpRrSequenceNumberTimeoutLocked(Timestamp now) {
    RTC_RUN_ON(&sequence_checker_);
    return ResetTimestampIfExpired(now, last_time_increased_sequence_number_,
                                 report_interval_);
}

} // namespace naivertc
