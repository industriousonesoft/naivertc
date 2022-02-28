#ifndef _RTC_CONGESTION_CONTROLLER_GOOG_CC_TRENDLINE_ESTIMATOR_H_
#define _RTC_CONGESTION_CONTROLLER_GOOG_CC_TRENDLINE_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/congestion_controller/goog_cc/delay_based/overuse_detector.hpp"

#include <deque>
#include <optional>

namespace naivertc {

// Heper class to detect the trandline of delay based the deltas calculated by `InterArrivalDelta`.
struct TrendlineEstimator {
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

    BandwidthUsage State() const;

    // Update the detector with a new sample.
    BandwidthUsage Update(double recv_delta_ms,
                          double send_delta_ms,
                          int64_t send_time_ms,
                          int64_t arrival_time_ms,
                          size_t packet_size);

private:
    BandwidthUsage UpdateTrendline(double recv_delta_ms,
                                   double send_delta_ms,
                                   int64_t send_time_ms,
                                   int64_t arrival_time_ms,
                                   size_t packet_size);

    struct PacketTiming;
    std::optional<double> CalcLinearFitSlope(const std::deque<PacketTiming>& samples) const;
    std::optional<double> CalcSlopeCap() const;

private:
    struct PacketTiming {
        PacketTiming(double arrival_time_ms,
                     double smoothed_delay_ms,
                     double accumulated_delay_ms) 
            : arrival_time_ms(arrival_time_ms),
              smoothed_delay_ms(smoothed_delay_ms),
              accumulated_delay_ms(accumulated_delay_ms) {}

        // This value is relative to the arrival time of the first packet.
        double arrival_time_ms;
        double smoothed_delay_ms;
        double accumulated_delay_ms;
    };
private:
    // Parameters
    const Configuration config_;
    // Smoothing coefficient.
    const double smoothing_coeff_;
    size_t num_samples_;
    // Keep the arrival times small by using the 
    // change from the first packet.
    int64_t first_arrival_time_ms_;
    // Exponential backoff filtering.
    double accumulated_delay_ms_;
    double smoothed_delay_ms_;

    // Delay histogram
    // Used for linear least squares regression.
    std::deque<PacketTiming> delay_hits_;

    OveruseDetector overuse_detector_;
    
    DISALLOW_COPY_AND_ASSIGN(TrendlineEstimator);
};
    
} // namespace naivertc


#endif