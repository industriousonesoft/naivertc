#ifndef _RTC_RTP_RTCP_RTP_FEC_RECEIVER_VIDEO_JITTER_JITTER_ESTIMATOR_H_
#define _RTC_RTP_RTCP_RTP_FEC_RECEIVER_VIDEO_JITTER_JITTER_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/rtt_filter.hpp"
#include "rtc/rtp_rtcp/components/rolling_accumulator.hpp"
#include "rtc/base/time/clock.hpp"

#include <memory>
#include <optional>

namespace naivertc {
namespace rtp {
namespace video {
namespace jitter {

// This class is not thread-safety, the caller MUST provide that.
// This class is used to estimate the jitter occurred during the transport of a frame.
class RTC_CPP_EXPORT JitterEstimator {
public:
    struct HyperParameters {
        // The filter factor used to calculate the average and variance of frame size.
        double filter_factor_of_frame_size = 0.97;
        // The weight used to estimate the max frame size.
        double weight_of_max_frame_size = 0.9999;
        // The max filter factor used to estimate the random jitter.
        double max_filter_factor_of_random_jitter = 0.9975; // (400 - 1) / 400
        // The min bandwidth required of transport channel.
        uint32_t min_bandwidth_bytes_per_second = 1000; // 1kb/s = 1000 bytes/s
        // If nack > nack_limit, we will add rrt into jitter estimate.
        uint32_t nack_limit = 3;
        // The number of standard deviaction of delay outlier
        int32_t num_std_dev_delay_outlier = 15;
        // The number of standard deviaction of frame size outlier
        int32_t num_std_dev_frame_size_outlier = 3;
        // The two parameters below used to estimate the threshold of noise linearly.
        double noise_std_devs = 2.33;
        double noise_std_dev_offset = 30.0;
        // The upper bound used to calculate the max time deviation based on noise. 
        double time_deviation_upper_bound = 3.5;
    };
public:
    // A constant describing the delay in ms from the jitter
    // buffer to the delay on the receiving side which is not
    // accounted for by the jitter buffer nor the decoding
    // delay estimate.
    static const uint32_t kOperatingSystemJitterMs = 10;
public:
    explicit JitterEstimator(const HyperParameters& hyper_params, 
                             Clock* clock);
    ~JitterEstimator();
    JitterEstimator& operator=(const JitterEstimator& rhs);

    void UpdateEstimate(int64_t frame_delay_ms, 
                        uint32_t frame_size,
                        bool incomplete_frame = false);

    void UpdateRtt(int64_t rtt_ms);

    // Updates the nack counter.
    void FrameNacked();

    int GetJitterEstimate(double rtt_multiplier,
                          std::optional<double> rtt_mult_add_cap_ms,
                          bool enable_reduced_delay = true);

    void Reset();
private:
    // Calculate difference in delay between a sample and the expected delay estimated
    // by the Kalman filter.
    double DeviationFromExpectedDelay(int64_t frame_delay_ms, int32_t frame_size_delta);

    // Estimates the random jitter by calculating the variance of the sample
    // distance from the line given by theta.
    void EstimateRandomJitter(double delay_deviation_ms, bool incomplete_frame = false);

    double EstimatedFrameRate() const;

    void KalmanEstimateChannel(int64_t frame_delay_ms, int32_t frame_size_delta);

    double CalcNoiseThreshold() const;
    double CalcJitterEstimate() const;

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
    // The max number of samples to estimate the random jitter.
    const double max_alpha_;
    const double theta_lower_bound_;
    const uint32_t nack_limit_;
    // The number of standard deviaction of delay outlier
    const int32_t num_std_dev_delay_outlier_;
    const int32_t num_std_dev_frame_size_outlier_;
    const double noise_std_devs_;
    const double noise_std_dev_offset_;
    const double time_deviation_upper_bound_;

    // Estimate covariance
    double theta_cov_[2][2];
    // A diagonal matrix, process noise convariance
    double Q_cov_[2][2];
    double avg_frame_size_;
    double var_frame_size_;
    double max_frame_size_;

    uint32_t frame_size_sum_;
    uint32_t prev_frame_size_;
    int64_t last_update_time_us_;
    // The previously returned jitter estimate
    double prev_estimated_jitter_ms_;
    // Average of the random jitter
    double avg_noise_;
    uint32_t sample_count_;
    // The filtered sum of jitter estimates
    double filtered_estimated_jitter_ms_;

    // Time in us when the latest nack was seen.
    int64_t latest_nack_time_us_;
    // Keeps track of the number of nacks received,
    // but never goes above `nack_limit_`
    uint32_t nack_count_;

    RttFilter rtt_filter_;
    RollingAccumulator<uint64_t> frame_delta_us_accumulator_;

    Clock* clock_;

};
    
} // namespace jitter
} // namespace video
} // namespace rtp 
} // namespace naivert 


#endif