#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_TRENDLINE_ESTIMATOR_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_TRENDLINE_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/congestion_controller/goog_cc/defines.hpp"

#include <deque>
#include <optional>

namespace naivertc {

struct RTC_CPP_EXPORT TrendlineEstimator {
public:
    static constexpr unsigned kDefaultTrendlineWindowSize = 20;

    struct Configuration {
        size_t beginning_packets = 7;
        size_t end_packets = 7;
        // Sort the packets in the window.
        bool enable_sort = false;
        // Cap the trendline slope based on the minimum delay seen
        // in the `beginning_packets` and `end_packets` respectively.
        bool enable_cap = false;
        double cap_uncertainty = 0.0;

        // Size in packdts of the window.
        size_t window_size = kDefaultTrendlineWindowSize;
    };
public:
    TrendlineEstimator(Configuration config);
    ~TrendlineEstimator();

    void Update(double recv_delta_ms,
                double send_delta_ms,
                int64_t send_time_ms,
                int64_t arrival_time_ms,
                size_t packet_size,
                bool calculated_deltas);

    BandwidthUsage State() const;

private:
    void UpdateTrendline(double recv_delta_ms,
                         double send_delta_ms,
                         int64_t send_time_ms,
                         int64_t arrival_time_ms,
                         size_t packet_size);

    void Detect(double trend, double ts_delta, int64_t now_ms);
    void UpdateThreshold(double modified_trend, int now_ms);

    std::optional<double> CalcLinearFitSlope() const;
    std::optional<double> CalcSlopeCap() const;

private:
    struct PacketTiming {
        PacketTiming(double arrival_time_span_ms,
                     double smoothed_delay_ms,
                     double accumulated_delay_ms) 
            : arrival_time_span_ms(arrival_time_span_ms),
              smoothed_delay_ms(smoothed_delay_ms),
              accumulated_delay_ms(accumulated_delay_ms) {}

        double arrival_time_span_ms;
        double smoothed_delay_ms;
        double accumulated_delay_ms;
    };
private:
    // Parameters
    const Configuration config_;
    // Smoothing coefficient.
    const double smoothing_coeff_;
    const double threshold_gain_;
    // Used by the existing threshold.
    int num_of_deltas_;
    // Keep the arrival times small by using the 
    // change from the first packet.
    int64_t first_arrival_time_ms_;
    // Exponential backoff filtering.
    double accumulated_delay_ms_;
    double smoothed_delay_ms_;
    // Linear least squares regression.
    std::deque<PacketTiming> delay_hits_;

    const double k_up_;
    const double k_down_;
    double overusing_time_threshold_;
    double threshold_;
    double prev_modified_trend_;
    int64_t last_update_ms_;
    double prev_trend_;
    double time_over_using_ms_;
    int overuse_counter_;
    BandwidthUsage estimated_state_;

    DISALLOW_COPY_AND_ASSIGN(TrendlineEstimator);
};
    
} // namespace naivertc


#endif