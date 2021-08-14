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
    report_interval_(config.rtcp_report_interval.value_or(TimeDelta::Millis(config.audio ? kDefaultAudioReportInterval
                                                                                         : kDefaultVideoReportInterval))) {
    
    if (!task_queue_) {
        task_queue_ = std::make_shared<TaskQueue>("RtcpSender.task.queue");
    }
}

RtcpSender::~RtcpSender() {}

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
