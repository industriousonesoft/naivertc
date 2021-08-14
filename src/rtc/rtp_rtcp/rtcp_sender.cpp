#include "rtc/rtp_rtcp/rtcp_sender.hpp"

#include <plog/Log.h>

namespace naivertc {
namespace {
const uint32_t kRtcpAnyExtendedReports = kRtcpXrReceiverReferenceTime |
                                         kRtcpXrDlrrReportBlock |
                                         kRtcpXrTargetBitrate;

constexpr int32_t kDefaultVideoReportInterval = 1000; // 1s
constexpr int32_t kDefaultAudioReportInterval = 5000; // 5s
}  // namespace

RtcpSender::RtcpSender(Configuration config, std::shared_ptr<TaskQueue> task_queue) 
    : audio_(config.audio),
    ssrc_(config.local_media_ssrc),
    clock_(config.clock),
    task_queue_(task_queue),
    report_interval_(config.rtcp_report_interval.value_or(
                     TimeDelta::Millis(config.audio ? kDefaultAudioReportInterval
                                                    : kDefaultVideoReportInterval))),
    max_packet_size_(kIpPacketSize - 28) {
    
    if (!task_queue_) {
        task_queue_ = std::make_shared<TaskQueue>("RtcpSender.task.queue");
    }

    InitBuilders();
}

RtcpSender::~RtcpSender() {}

uint32_t RtcpSender::ssrc() const {
    return task_queue_->Sync<uint32_t>([this](){
        return this->ssrc_;
    });
}

void RtcpSender::set_ssrc(uint32_t ssrc) {
    task_queue_->Async([this, ssrc](){
        this->ssrc_ = ssrc;
    });
}

void RtcpSender::set_remote_ssrc(uint32_t ssrc) {
    task_queue_->Async([this, ssrc](){
        this->remote_ssrc_ = ssrc;
    });
}

void RtcpSender::set_cname(std::string cname) {
    assert(cname.size() < kRtcpCNameSize);
    task_queue_->Async([this, cname=std::move(cname)](){
        this->cname_ = cname;
    });
}

void RtcpSender::set_max_rtp_packet_size(size_t max_packet_size) {
    task_queue_->Async([this, max_packet_size]{
        this->max_packet_size_ = max_packet_size;
    });
}

void RtcpSender::set_csrcs(const std::vector<uint32_t>& csrcs) {
    task_queue_->Async([this, csrcs=std::move(csrcs)](){
        this->csrcs_ = std::move(csrcs);
    });
}

void RtcpSender::SetRtpClockRate(int8_t rtp_payload_type, int rtp_clock_rate_hz) {
    task_queue_->Async([this, rtp_payload_type, rtp_clock_rate_hz](){
        this->rtp_clock_rates_khz_[rtp_payload_type] = rtp_clock_rate_hz / 1000;
    });
}

void RtcpSender::SetTimestampOffset(uint32_t timestamp_offset) {
    task_queue_->Async([this, timestamp_offset](){
        this->timestamp_offset_ = timestamp_offset;
    });
}

void RtcpSender::SetLastRtpTime(uint32_t rtp_timestamp,
                                std::optional<Timestamp> capture_time,
                                std::optional<int8_t> rtp_payload_type) {
    task_queue_->Async([this, rtp_timestamp, capture_time, rtp_payload_type](){
        if (rtp_payload_type.has_value()) {
            this->last_rtp_payload_type_ = rtp_payload_type.value();
        }
        this->last_rtp_timestamp_ = rtp_timestamp;
        if (!capture_time.has_value()) {
            last_frame_capture_time_ = this->clock_->CurrentTime();
        }else {
            last_frame_capture_time_ = capture_time;
        }
    });
}

bool RtcpSender::Sending() const {
    return task_queue_->Sync<bool>([this](){
        return this->sending_;
    });
}
    
void RtcpSender::SetSendingStatus(const FeedbackState& feedback_state, bool enable) {
    task_queue_->Async([this, feedback_state, enable](){
        bool send_rtcp_bye = false;
        if (enable == false && this->sending_ == true) {
            send_rtcp_bye = true;
        }
        this->sending_ = enable;
        if (send_rtcp_bye) {
            // TODO: send RTCP bye packet
        }
    });
}

void RtcpSender::SetRemb(uint64_t bitrate_bps, std::vector<uint32_t> ssrcs) {
    task_queue_->Async([this, bitrate_bps, ssrcs=std::move(ssrcs)](){
        this->remb_bitrate_ = bitrate_bps;
        this->remb_ssrcs_ = std::move(ssrcs);

        this->SetFlag(kRtcpRemb, false);
        // Send a REMB immediately if we have a new REMB. The frequency of REMBs is
        // throttled by the caller.
        this->SetNextRtcpSendEvaluationDuration(TimeDelta::Zero());
    });
}

bool RtcpSender::SendLossNotification(const FeedbackState& feedback_state,
                                      uint16_t last_decoded_seq_num,
                                      uint16_t last_received_seq_num,
                                      bool decodability_flag,
                                      bool buffering_allowed) {
    return task_queue_->Sync<bool>([=, &feedback_state](){
        bool bRet = false;
        auto callback = [&](BinaryBuffer packet) {
            // TODO: Send RTCP packet by transport
        };
        
        if (!this->loss_notification_.Set(last_decoded_seq_num, last_received_seq_num, decodability_flag)) {
            return false;
        }

        this->SetFlag(kRtcpLossNotification, true);

        // The loss notification will be batched with additional feedback messages.
        if (buffering_allowed) {
            return true;
        }

        PacketSender sender(callback, max_packet_size_);
        bRet = ComputeCompoundRtcpPacket(feedback_state, RtcpPacketType::kRtcpLossNotification, {}, sender);
        if (bRet) {
            sender.Send();
        }
        return bRet;
    });
}

// Private methods
void RtcpSender::SetNextRtcpSendEvaluationDuration(TimeDelta duration) {
    next_time_to_send_rtcp_ = clock_->CurrentTime() + duration;
}

void RtcpSender::SetFlag(uint32_t type, bool is_volatile) {
    if (type & kRtcpAnyExtendedReports) {
        report_flags_.insert(ReportFlag(kRtcpAnyExtendedReports, is_volatile));
    } else {
        report_flags_.insert(ReportFlag(type, is_volatile));
    }
}

bool RtcpSender::IsFlagPresent(uint32_t type) const {
    return report_flags_.find(ReportFlag(type, false)) != report_flags_.end();
}

bool RtcpSender::ConsumeFlag(uint32_t type, bool forced) {
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
