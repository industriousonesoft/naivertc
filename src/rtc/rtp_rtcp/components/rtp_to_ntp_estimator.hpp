#ifndef _RTC_RTP_RTCP_COMPONENTS_TIME_RTP_TO_NTP_ESTIMATOR_H_
#define _RTC_RTP_RTCP_COMPONENTS_TIME_RTP_TO_NTP_ESTIMATOR_H_

#include "base/defines.hpp"
#include "rtc/base/time/ntp_time.hpp"
#include "rtc/rtp_rtcp/components/num_unwrapper.hpp"

#include <list>
#include <optional>

namespace naivertc {

// Class for converting an RTP timestamp to the NTP domain in milliseconds.
class RTC_CPP_EXPORT RtpToNtpEstimator {
public:
    struct Parameters {
        Parameters() 
            : frequency_khz(0.0), offset_ms(0.0) {}
        Parameters(double frequency_khz, double offset_ms) 
            : frequency_khz(frequency_khz), offset_ms(offset_ms) {}

        double frequency_khz;
        double offset_ms;
    };
public:
    RtpToNtpEstimator();
    ~RtpToNtpEstimator();

    bool UpdateMeasurements(uint32_t ntp_secs, uint32_t ntp_frac, uint32_t rtp_timestamp);

    // Converts an RTP timestamp to the NTP domain in milliseconds.
    std::optional<int64_t> Estimate(int64_t rtp_timestamp) const;

    const std::optional<Parameters> params() const { return params_; };

    // Maximum number of consecutively invalid RTCP SR reports.
    static const int kMaxInvalidSamples = 3;
private:
    struct Measurement {
        Measurement(int64_t ntp_time_ms, 
                    int64_t unwrapped_rtp_timestamp);
        bool operator==(const Measurement& other) const;

        int64_t ntp_time_ms;
        int64_t unwrapped_rtp_timestamp;
    };
private:
    void CalculateParameters();
    bool Contains(const Measurement& measurement) const;
    bool LinearRegression(double* k, double* b) const;
private:
    int consecutive_invalid_samples_;
    std::list<Measurement> measurements_;
    std::optional<Parameters> params_;
    mutable TimestampUnwrapper timestamp_unwrapper_;
};

} // namespace naivertc

#endif