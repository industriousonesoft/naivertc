#include "rtc/rtp_rtcp/components/rtp_to_ntp_estimator.hpp"
#include "common/utils_random.hpp"
#include <gtest/gtest.h>

namespace naivertc {
namespace test {
namespace {
constexpr uint32_t kOneMsInNtpFrac = 4294967;
constexpr uint32_t kOneHourInNtpSec = 60 * 60;
constexpr uint32_t kTimestampTicksPerMs = 90;
}  // namespace

TEST(WrapAroundTests, OldRtcpWrapped_OldRtpTimestamp) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 0;
    uint32_t ntp_frac = 1;
    uint32_t timestamp = 0;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    ntp_frac += kOneMsInNtpFrac;
    timestamp -= kTimestampTicksPerMs;
    // No wraparound will be detected, since we are not allowed to wrap below 0,
    // but there will be huge rtp timestamp jump, which should be detected.
    EXPECT_FALSE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
}

TEST(WrapAroundTests, OldRtcpWrapped_OldRtpTimestamp_Wraparound_Detected) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 0;
    uint32_t ntp_frac = 1;
    uint32_t timestamp = 0xFFFFFFFE;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    ntp_frac += 2 * kOneMsInNtpFrac;
    timestamp += 2 * kTimestampTicksPerMs;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    ntp_frac += kOneMsInNtpFrac;
    timestamp -= kTimestampTicksPerMs;
    // Expected to fail since the newer RTCP has a smaller RTP timestamp than the older.
    EXPECT_FALSE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
}

TEST(WrapAroundTests, NewRtcpWrapped) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 0;
    uint32_t ntp_frac = 1;
    uint32_t timestamp = 0xFFFFFFFF;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    ntp_frac += kOneMsInNtpFrac;
    timestamp += kTimestampTicksPerMs;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    std::optional<int64_t> timestamp_ms = estimator.Estimate(0xFFFFFFFF);
    EXPECT_TRUE(timestamp_ms.has_value());
    // Since this RTP packet has the same timestamp as the RTCP packet constructed
    // at time 0 it should be mapped to 0 as well.
    EXPECT_EQ(0, timestamp_ms.value());
}

TEST(WrapAroundTests, RtpWrapped) {
    RtpToNtpEstimator estimator;

    uint32_t ntp_sec = 0;
    uint32_t ntp_frac = 1;
    uint32_t timestamp = 0xFFFFFFFF - 2 * kTimestampTicksPerMs;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    ntp_frac += kOneMsInNtpFrac;
    timestamp += kTimestampTicksPerMs;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    auto timestamp_ms = estimator.Estimate(0xFFFFFFFF - 2 * kTimestampTicksPerMs);
    EXPECT_TRUE(timestamp_ms.has_value());
    // Since this RTP packet has the same timestamp as the RTCP packet constructed
    // at time 0 it should be mapped to 0 as well.
    EXPECT_EQ(0, timestamp_ms.value());
    // Two kTimestampTicksPerMs advanced.
    timestamp += kTimestampTicksPerMs;
    timestamp_ms = estimator.Estimate(timestamp);
    EXPECT_TRUE(timestamp_ms.has_value());
    EXPECT_EQ(2, timestamp_ms.value());
    // Wrapped rtp.
    timestamp += kTimestampTicksPerMs;
    timestamp_ms = estimator.Estimate(timestamp);
    EXPECT_TRUE(timestamp_ms.has_value());
    EXPECT_EQ(3, timestamp_ms.value());
}

TEST(WrapAroundTests, OldRtp_RtcpsWrapped) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 0;
    uint32_t ntp_frac = 1;
    uint32_t timestamp = 0xFFFFFFFF;

    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
    ntp_frac += kOneMsInNtpFrac;
    timestamp += kTimestampTicksPerMs;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
    timestamp -= 2 * kTimestampTicksPerMs;
    int64_t timestamp_ms = 0xFFFFFFFF;
    EXPECT_FALSE(estimator.Estimate(timestamp).has_value());
}

TEST(WrapAroundTests, OldRtp_NewRtcpWrapped) {
    RtpToNtpEstimator estimator;
   
    uint32_t ntp_sec = 0;
    uint32_t ntp_frac = 1;
    uint32_t timestamp = 0xFFFFFFFF;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
    ntp_frac += kOneMsInNtpFrac;
    timestamp += kTimestampTicksPerMs;

    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
    timestamp -= kTimestampTicksPerMs;

    auto timestamp_ms = estimator.Estimate(timestamp);
    EXPECT_TRUE(timestamp_ms.has_value());
    // Constructed at the same time as the first RTCP and should therefore be
    // mapped to zero.
    EXPECT_EQ(0, timestamp_ms.value());
}

TEST(WrapAroundTests, GracefullyHandleRtpJump) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 0;
    uint32_t ntp_frac = 1;
    uint32_t timestamp = 0;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    ntp_frac += kOneMsInNtpFrac;
    timestamp += kTimestampTicksPerMs;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    ntp_frac += kOneMsInNtpFrac;
    timestamp -= kTimestampTicksPerMs;
    auto timestamp_ms = estimator.Estimate(timestamp);
    EXPECT_TRUE(timestamp_ms.has_value());
    // Constructed at the same time as the first RTCP and should therefore be
    // mapped to zero.
    EXPECT_EQ(0, timestamp_ms.value());

    timestamp -= 0xFFFFF;
    for (int i = 0; i < RtpToNtpEstimator::kMaxInvalidSamples - 1; ++i) {
        EXPECT_FALSE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
        ntp_frac += kOneMsInNtpFrac;
        timestamp += kTimestampTicksPerMs;
    }
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    ntp_frac += kOneMsInNtpFrac;
    timestamp += kTimestampTicksPerMs;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
    ntp_frac += kOneMsInNtpFrac;
    timestamp += kTimestampTicksPerMs;

    timestamp_ms = estimator.Estimate(timestamp);
    EXPECT_TRUE(timestamp_ms.has_value());
    // 6 milliseconds has passed since the start of the test.
    EXPECT_EQ(6, timestamp_ms.value());
}

TEST(UpdateRtcpMeasurementTests, FailsForZeroNtp) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 0;
    uint32_t ntp_frac = 0;
    uint32_t timestamp = 0x12345678;
    EXPECT_FALSE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
}

TEST(UpdateRtcpMeasurementTests, FailsForEqualNtp) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 0;
    uint32_t ntp_frac = 699925050;
    uint32_t timestamp = 0x12345678;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
    // Ntp time already added, list not updated.
    ++timestamp;
    EXPECT_FALSE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
}

TEST(UpdateRtcpMeasurementTests, FailsForOldNtp) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 1;
    uint32_t ntp_frac = 699925050;
    uint32_t timestamp = 0x12345678;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
    // Old ntp time, list not updated.
    ntp_frac -= kOneMsInNtpFrac;
    timestamp += kTimestampTicksPerMs;
    EXPECT_FALSE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
}

TEST(UpdateRtcpMeasurementTests, FailsForTooNewNtp) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 1;
    uint32_t ntp_frac = 699925050;
    uint32_t timestamp = 0x12345678;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    // Ntp time from far future, list not updated.
    ntp_sec += kOneHourInNtpSec * 2;
    timestamp += kTimestampTicksPerMs * 10;
    EXPECT_FALSE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
}

TEST(UpdateRtcpMeasurementTests, FailsForEqualTimestamp) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 0;
    uint32_t ntp_frac = 2;
    uint32_t timestamp = 0x12345678;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    // Timestamp already added, list not updated.
    ++ntp_frac;
    EXPECT_FALSE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
}

TEST(UpdateRtcpMeasurementTests, FailsForOldRtpTimestamp) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 0;
    uint32_t ntp_frac = 2;
    uint32_t timestamp = 0x12345678;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
   
    // Old timestamp, list not updated.
    ntp_frac += kOneMsInNtpFrac;
    timestamp -= kTimestampTicksPerMs;
    EXPECT_FALSE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
}

TEST(UpdateRtcpMeasurementTests, VerifyParameters) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 1;
    uint32_t ntp_frac = 2;
    uint32_t timestamp = 0x12345678;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
    EXPECT_FALSE(estimator.params());
    // Add second report, parameters should be calculated.
    ntp_frac += kOneMsInNtpFrac;
    timestamp += kTimestampTicksPerMs;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));
    EXPECT_TRUE(estimator.params());
    EXPECT_DOUBLE_EQ(90.0, estimator.params()->frequency_khz);
    EXPECT_NE(0.0, estimator.params()->offset_ms);
}

TEST(RtpToNtpTests, FailsForNoParameters) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 1;
    uint32_t ntp_frac = 2;
    uint32_t timestamp = 0x12345678;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    // Parameters are not calculated, conversion of RTP to NTP time should fail.
    EXPECT_FALSE(estimator.params());
    EXPECT_FALSE(estimator.Estimate(timestamp).has_value());
}

TEST(RtpToNtpTests, AveragesErrorOut) {
    RtpToNtpEstimator estimator;
    uint32_t ntp_sec = 1;
    uint32_t ntp_frac = 90000000;  // More than 1 ms.
    uint32_t timestamp = 0x12345678;
    const int kNtpSecStep = 1;  // 1 second.
    const int kRtpTicksPerMs = 90;
    const int kRtpStep = kRtpTicksPerMs * 1000;
    EXPECT_TRUE(estimator.UpdateMeasurements(ntp_sec, ntp_frac, timestamp));

    for (size_t i = 0; i < 1000; i++) {
        // Advance both timestamps by exactly 1 second.
        ntp_sec += kNtpSecStep;
        timestamp += kRtpStep;
        // Add upto 1ms of errors to NTP and RTP timestamps passed to estimator.
        EXPECT_TRUE(estimator.UpdateMeasurements(
            ntp_sec,
            ntp_frac + naivertc::utils::random::random<int>(-static_cast<int>(kOneMsInNtpFrac), static_cast<int>(kOneMsInNtpFrac)),
            timestamp + naivertc::utils::random::random<int>(-kRtpTicksPerMs, kRtpTicksPerMs)));
       
        auto estimated_ntp_ms = estimator.Estimate(timestamp);
        EXPECT_TRUE(estimated_ntp_ms.has_value());
        // Allow upto 2 ms of error.
        EXPECT_NEAR(NtpTime(ntp_sec, ntp_frac).ToMs(), estimated_ntp_ms.value(), 2);
    }
}

} // namespace test
} // namespace naivertc