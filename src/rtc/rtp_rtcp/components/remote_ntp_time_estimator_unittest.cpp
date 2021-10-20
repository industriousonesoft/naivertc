#include "rtc/rtp_rtcp/components/remote_ntp_time_estimator.hpp"
#include "rtc/base/time/clock_simulated.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {
namespace {
constexpr int64_t kTestRtt = 10;
constexpr int64_t kLocalClockInitialTimeMs = 123;
constexpr int64_t kRemoteClockInitialTimeMs = 345;
constexpr uint32_t kTimestampOffset = 567;
constexpr int64_t kRemoteToLocalClockOffsetMs = kLocalClockInitialTimeMs - kRemoteClockInitialTimeMs;
} // namespace

class RTP_RTCP_RemoteNtpTimeEstimatorTest : public ::testing::Test {
protected:
    RTP_RTCP_RemoteNtpTimeEstimatorTest()
        : local_clock_(std::make_shared<SimulatedClock>(kLocalClockInitialTimeMs * 1000)),
          remote_clock_(std::make_shared<SimulatedClock>(kRemoteClockInitialTimeMs * 1000)),
          estimator_(new RemoteNtpTimeEstimator(local_clock_)) {}

    ~RTP_RTCP_RemoteNtpTimeEstimatorTest() override = default;

    void AdvanceTimeMilliseconds(int64_t ms) {
        local_clock_->AdvanceTimeMilliseconds(ms);
        remote_clock_->AdvanceTimeMilliseconds(ms);
    }

    uint32_t GetRemoteTimestamp() {
        return static_cast<uint32_t>(remote_clock_->now_ms()) * 90 + kTimestampOffset;
    }

    NtpTime GetRemoteNtpTime() { return remote_clock_->CurrentNtpTime(); }

    void SendRtcpSr() {
        uint32_t rtcp_timestamp = GetRemoteTimestamp();
        NtpTime ntp = GetRemoteNtpTime();

        AdvanceTimeMilliseconds(kTestRtt / 2);
        ReceiveRtcpSr(kTestRtt, rtcp_timestamp, ntp.seconds(), ntp.fractions());
    }

    void SendRtcpSrInaccurately(int64_t ntp_error_ms,
                              int64_t networking_delay_ms) {
        uint32_t rtcp_timestamp = GetRemoteTimestamp();
        int64_t ntp_error_fractions = ntp_error_ms * static_cast<int64_t>(NtpTime::kFractionsPerSecond) / 1000;
        NtpTime ntp(static_cast<uint64_t>(GetRemoteNtpTime()) + ntp_error_fractions);
        AdvanceTimeMilliseconds(kTestRtt / 2 + networking_delay_ms);
        ReceiveRtcpSr(kTestRtt, rtcp_timestamp, ntp.seconds(), ntp.fractions());
    }

    void UpdateRtcpTimestamp(int64_t rtt,
                             uint32_t ntp_secs,
                             uint32_t ntp_frac,
                             uint32_t rtp_timestamp,
                             bool expected_result) {
        EXPECT_EQ(expected_result, estimator_->UpdateTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp));
    }

    void ReceiveRtcpSr(int64_t rtt,
                       uint32_t rtcp_timestamp,
                       uint32_t ntp_seconds,
                       uint32_t ntp_fractions) {
        UpdateRtcpTimestamp(rtt, ntp_seconds, ntp_fractions, rtcp_timestamp, true);
    }

    std::shared_ptr<SimulatedClock> local_clock_;
    std::shared_ptr<SimulatedClock> remote_clock_;
    std::unique_ptr<RemoteNtpTimeEstimator> estimator_;
};

TEST_F(RTP_RTCP_RemoteNtpTimeEstimatorTest, Estimate) {
    // Failed without valid NTP.
    UpdateRtcpTimestamp(kTestRtt, 0, 0, 0, false);

    AdvanceTimeMilliseconds(1000);
    // Remote peer sends first RTCP SR.
    SendRtcpSr();

    // Remote sends a RTP packet.
    AdvanceTimeMilliseconds(15);
    uint32_t rtp_timestamp = GetRemoteTimestamp();
    int64_t capture_ntp_time_ms = local_clock_->CurrentNtpTime().ToMs();

    // Local peer needs at least 2 RTCP SR to calculate the capture time.
    const int64_t kNotEnoughRtcpSr = -1;
    EXPECT_EQ(kNotEnoughRtcpSr, estimator_->Estimate(rtp_timestamp));
    EXPECT_EQ(std::nullopt, estimator_->EstimatedOffsetInMs());

    AdvanceTimeMilliseconds(800);
    // Remote sends second RTCP SR.
    SendRtcpSr();

    // Local peer gets enough RTCP SR to calculate the capture time.
    EXPECT_EQ(capture_ntp_time_ms, estimator_->Estimate(rtp_timestamp));
    EXPECT_EQ(kRemoteToLocalClockOffsetMs, estimator_->EstimatedOffsetInMs());
}

TEST_F(RTP_RTCP_RemoteNtpTimeEstimatorTest, AveragesErrorsOut) {
    // Remote peer sends first 10 RTCP SR without errors.
    for (int i = 0; i < 10; ++i) {
        AdvanceTimeMilliseconds(1000);
        SendRtcpSr();
    }

    AdvanceTimeMilliseconds(150);
    uint32_t rtp_timestamp = GetRemoteTimestamp();
    int64_t capture_ntp_time_ms = local_clock_->CurrentNtpTime().ToMs();
    // Local peer gets enough RTCP SR to calculate the capture time.
    EXPECT_EQ(capture_ntp_time_ms, estimator_->Estimate(rtp_timestamp));
    EXPECT_EQ(kRemoteToLocalClockOffsetMs, estimator_->EstimatedOffsetInMs());

    // Remote sends corrupted RTCP SRs
    AdvanceTimeMilliseconds(1000);
    SendRtcpSrInaccurately(/*ntp_error_ms=*/2, /*networking_delay_ms=*/-1);
    AdvanceTimeMilliseconds(1000);
    SendRtcpSrInaccurately(/*ntp_error_ms=*/-2, /*networking_delay_ms=*/1);

    // New RTP packet to estimate timestamp.
    AdvanceTimeMilliseconds(150);
    rtp_timestamp = GetRemoteTimestamp();
    capture_ntp_time_ms = local_clock_->CurrentNtpTime().ToMs();

    // Errors should be averaged out.
    EXPECT_EQ(capture_ntp_time_ms, estimator_->Estimate(rtp_timestamp));
    EXPECT_EQ(kRemoteToLocalClockOffsetMs, estimator_->EstimatedOffsetInMs());
}

} // namespace test
} // namespace naivertc