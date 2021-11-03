#ifndef _RTC_RTP_RTCP_RTP_FEC_RECEIVER_VIDEO_JITTER_JITTER_ESTIMATOR_H_
#define _RTC_RTP_RTCP_RTP_FEC_RECEIVER_VIDEO_JITTER_JITTER_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/rtt_filter.hpp"
#include "rtc/rtp_rtcp/components/rolling_accumulator.hpp"
#include "rtc/base/time/clock.hpp"

#include <memory>

namespace naivertc {
namespace rtc {
namespace video {
namespace jitter {

class RTC_CPP_EXPORT JitterEstimator {
public:
    explicit JitterEstimator(std::shared_ptr<Clock> clock);
    ~JitterEstimator();

    void UpdateEstimate(int64_t frame_delay_ms, 
                        uint32_t frame_size, 
                        bool incomplete_frame = false);

    void Reset();
private:
    // Calculate difference in delay between a sample and the expected delay estimated
    // by the Kalman filter.
    double DeviationFromExpectedDelay(int64_t frame_delay_ms, int32_t frame_size_delta);

    // Estimates the random jitter by calculating the variance of the sample
    // distance from the line given by theta.
    void EstimateRandomJitter(double d_dT, bool incomplete_frame);

    double EstimatedFrameRate() const;

protected:
    // Estimated line parameters (slope, offset);
    double theta_[2];
    // Variance of the time-deviation from the line.
    double var_noise_;

private:
    // χ phai
    const double phi_;
    // ψ psai
    const double psi_;
    const uint32_t sample_count_max_;
    const double theta_low_;
    const uint32_t nack_limit_;
    // The number of standard deviaction of delay outlier
    const int32_t num_std_dev_delay_outlier_;
    const int32_t num_std_dev_frame_size_outlier_;
    const double noise_std_devs_;
    const double noise_std_dev_offset_;
    const double time_deviation_upper_bound_;
    const bool enable_reduced_delay_;

    // Estimate covariance
    double theta_cov_[2][2];
    // A diagonal matrix, process noise convariance
    double Q_cov_[2][2];
    double avg_frame_size_;
    double var_frame_size_;
    double max_frame_size_;
    
    uint32_t frame_count_;
    uint32_t frame_size_sum_;
    uint32_t prev_frame_size_;
    int64_t last_update_time_us_;
    // The previously returned jitter estimate
    double prev_estimate_;
    // Average of the random jitter
    double avg_noise_;
    uint32_t sample_count_;
    // The filtered sum of jitter estimates
    double filtered_sum_of_jitter_estimates_;

    uint32_t startup_count_;
    // Timestamp in ms when the latest nack was seen.
    int64_t latest_nack_timestamp_;
    // Keeps track of the number of nacks received,
    // but never goes above `nack_limit_`
    uint32_t nack_count_;

    RttFilter rtt_filter_;
    RollingAccumulator<uint64_t> frame_delta_us_accumulator_;

    std::shared_ptr<Clock> clock_;

};
    
} // namespace jitter
} // namespace video
} // namespace rtc
} // namespace naivert 


#endif