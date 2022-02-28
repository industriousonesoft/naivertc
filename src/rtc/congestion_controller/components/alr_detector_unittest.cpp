#include "rtc/congestion_controller/components/alr_detector.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

#include <optional>

namespace naivertc {
namespace test {
namespace {

constexpr TimeDelta kWindow = TimeDelta::Millis(500); // 500ms
constexpr DataRate kTargetBitrate = DataRate::KilobitsPerSec(300); // 300kbps
constexpr TimeDelta kTimeStep = TimeDelta::Millis(10); // 10ms

size_t IntervalBytes(const DataRate& bitrate, const TimeDelta& interval) {
    return static_cast<size_t>((bitrate.kbps() * interval.ms()) / 8);
}

} // namespace

class T(AlrDetectorTest) : public ::testing::Test {
public:
    T(AlrDetectorTest)() 
        : clock_(1000'000) {}
    ~T(AlrDetectorTest)() override = default;

    void SetUp() override {
        alr_decector_ = std::make_unique<AlrDetector>(alr_config_, &clock_);
        alr_decector_->SetTargetBitrate(kTargetBitrate);
    }

    void ProduceTraffic(TimeDelta interval, double bw_usage_ratio){
        if (interval.IsZero() || bw_usage_ratio < 0) {
            return;
        }
        auto end_time = clock_.CurrentTime() + interval;
        while (clock_.CurrentTime() <= end_time) {
            clock_.AdvanceTime(kTimeStep);
            alr_decector_->OnBytesSent(IntervalBytes(kTargetBitrate * bw_usage_ratio, kTimeStep), clock_.CurrentTime());
        }
        auto remaining_time_ms = interval.ms<int>() % kTimeStep.ms<int>();
        if (remaining_time_ms > 0) {
            clock_.AdvanceTime(kTimeStep);
            alr_decector_->OnBytesSent(IntervalBytes(kTargetBitrate * bw_usage_ratio, kTimeStep), clock_.CurrentTime());
        }
    }

protected:
    SimulatedClock clock_;
    AlrDetector::Configuration alr_config_;
    std::unique_ptr<AlrDetector> alr_decector_;
};

MY_TEST_F(AlrDetectorTest, AlrDetection) {
    // Start in non-ALR region.
    EXPECT_FALSE(alr_decector_->InAlr());

    // Stay in non-ALR region when bandwidth usage is close to 100%.
    ProduceTraffic(kWindow, 0.9);
    EXPECT_FALSE(alr_decector_->InAlr());

    // Starts ALR when bandwidth usage drops below 20%.
    ProduceTraffic(kWindow * 2, 0.2);
    EXPECT_TRUE(alr_decector_->InAlr());

    // Ends ALR when usage is above 65%.
    ProduceTraffic(kWindow * 2, 0.98);
    EXPECT_FALSE(alr_decector_->InAlr());
}

MY_TEST_F(AlrDetectorTest, ShortSpike) {
    // Start in non-ALR region.
    EXPECT_FALSE(alr_decector_->InAlr());

    // Starts ALR when bandwidth usage drops below 20%.
    ProduceTraffic(kWindow * 2, 0.2);
    EXPECT_TRUE(alr_decector_->InAlr());

    // Verify that we stay in ALR even after a short (100ms) bitrate spike.
    ProduceTraffic(kWindow / 5, 1.5);
    EXPECT_TRUE(alr_decector_->InAlr());

    // Ends ALR when usage is above 65%.
    ProduceTraffic(kWindow * 2, 0.95);
    EXPECT_FALSE(alr_decector_->InAlr());
}

MY_TEST_F(AlrDetectorTest, BandwidthEstimateChanges) {
    // Start in non-ALR region.
    EXPECT_FALSE(alr_decector_->InAlr());

    // Starts ALR when bandwidth usage drops below 20%.
    ProduceTraffic(kWindow * 2, 0.2);
    EXPECT_TRUE(alr_decector_->InAlr());

    // When bandwidth estimate drops, the detector should
    // stay in ALR mode.
    alr_decector_->SetTargetBitrate(kTargetBitrate * 0.5);
    EXPECT_TRUE(alr_decector_->InAlr());

    // Quit ALR mode as the sender continues sending the same
    // amount of traffic. This is necessary to ensure that 
    // ProbeController can still react to the BWE drop by 
    // initiating a new probe.
    ProduceTraffic(kWindow * 2, 0.5);
    EXPECT_FALSE(alr_decector_->InAlr());
}

MY_TEST_F(AlrDetectorTest, ConfigAlrDetector) {
    // Reconfig ALR detector.
    alr_config_.bandwidth_usage_ratio = 0.9;
    alr_config_.start_budget_level_ratio = 0.0;
    alr_config_.stop_budget_level_ratio = -0.1;
    SetUp();

    // Start in non-ALR region.
    EXPECT_FALSE(alr_decector_->InAlr());

    // ALR does not start at 100% utilization (Overused 10%).
    ProduceTraffic(kWindow * 2, 1);
    EXPECT_FALSE(alr_decector_->InAlr());

    // ALR does start at 85% utilization (Undersed 5%).
    // Overused 10% above so it should take about 2s to reach a budget level of
    // 0%.
    ProduceTraffic(TimeDelta::Millis(2100), 0.85);
    EXPECT_TRUE(alr_decector_->InAlr());
}
    
} // namespace test    
} // namespace naivertc
