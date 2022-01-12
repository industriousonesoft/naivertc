#include "rtc/rtp_rtcp/components/rtp_stream_statistician.hpp"

namespace naivertc {
namespace {
constexpr int64_t kStatisticsTimeoutMs = 8000;
constexpr int64_t kStatisticsProcessIntervalMs = 1000;

}  // namespace

RtpStreamStatistician::RtpStreamStatistician(uint32_t ssrc, Clock* clock, int max_reordering_threshold) 
    : ssrc_(ssrc),
      clock_(clock),
      delta_internal_unix_epoch_ms_(/*time based on Unix epoch*/(clock_->now_ntp_time_ms() - kNtpJan1970Ms) - clock_->now_ms()) {}
    
RtpStreamStatistician::~RtpStreamStatistician() = default;
    
} // namespace naivertc
