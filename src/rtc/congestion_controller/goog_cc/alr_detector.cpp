#include "rtc/congestion_controller/goog_cc/alr_detector.hpp"
#include "rtc/base/time/clock.hpp"

namespace naivertc {

AlrDetector::AlrDetector(Configuration config, Clock* clock) 
    : config_(std::move(config)),
      clock_(clock),
      alr_budget_(DataRate::Zero(), true) {}
    
AlrDetector::~AlrDetector() {}

void AlrDetector::OnBytesSent(size_t bytes_sent, Timestamp send_time) {
    if (!last_send_time_) {
        last_send_time_ = send_time;
        return;
    }
    TimeDelta interval_time = send_time - *last_send_time_;
    last_send_time_ = send_time;

    // If the consumed(sent) bytes are more than the 
    // increased bytes, which indicates the ALR is stoping.
    // In other words, the send bandwidth usage is greater
    // than the |config_.bandwidth_usage_ratio|.
    alr_budget_.ConsumeBudget(bytes_sent);
    alr_budget_.IncreaseBudget(interval_time);
    auto alr_budget_ratio = alr_budget_.budget_ratio();
    // Detect a new ALR starts when bandwidth usage is below 20%. 
    if (!alr_started_time_ && alr_budget_ratio > config_.start_budget_level_ratio) {
        alr_started_time_.emplace(clock_->CurrentTime());
        alr_ended_time_.reset();
    // Detect a new ALR ends when bandwidth usage is above 50%.
    } else if (alr_started_time_ && !alr_ended_time_ && alr_budget_ratio < config_.stop_budget_level_ratio) {
        alr_started_time_.reset();
        alr_ended_time_.emplace(clock_->CurrentTime());
    }

}
    
void AlrDetector::OnEstimate(DataRate bitrate) {
    alr_budget_.set_target_bitrate(bitrate * config_.bandwidth_usage_ratio);
}
    
} // namespace naivertc