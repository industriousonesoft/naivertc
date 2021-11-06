#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_estimator.hpp"
#include "rtc/base/time/clock_simulated.hpp"
#include "rtc/base/numerics/histogram_percentile_counter.hpp"
#include "common/utils_time.hpp"

#include <gtest/gtest.h>

using namespace naivertc::rtp::video::jitter;

namespace naivertc {
namespace test {

// JitterEstimatorTest
class JitterEstimatorTest : public ::testing::Test {
protected:
    JitterEstimatorTest() 
        : fake_clock_(std::make_shared<SimulatedClock>(0)), 
          jitter_estimator_(std::make_unique<JitterEstimator>(JitterEstimator::HyperParameters(), fake_clock_)) {}

    void Reset(const JitterEstimator::HyperParameters& hyper_params) {
        jitter_estimator_.reset(new JitterEstimator(hyper_params, fake_clock_));
    }

protected:
    std::shared_ptr<SimulatedClock> fake_clock_;
    std::unique_ptr<JitterEstimator> jitter_estimator_;
};

// SampleGenerator
// Generates some simple test data in the form of a sawtooth wave.
class SampleGenerator {
public:
    explicit SampleGenerator(int32_t amplitude) 
        : amplitude_(amplitude), counter_(0) {}
    ~SampleGenerator() = default;

    int64_t Delay() const { return ((counter_ % 11) - 5) * amplitude_; }

    uint32_t FrameSize() const { return 1000 + Delay(); }

    void Advance() { ++counter_; }

private:
    const int32_t amplitude_;
    int64_t counter_;
};

// 5 fps, disable jitter delay altogether.
TEST_F(JitterEstimatorTest, TestLowFrameRate) {
    SampleGenerator gen(10);
    int fps = 5;
    int64_t frame_delta_us = kNumMicrosecsPerSec / fps;
    for (int i = 0; i < 60; ++i) {
        jitter_estimator_->UpdateEstimate(gen.Delay(), gen.FrameSize());
        fake_clock_->AdvanceTimeUs(frame_delta_us);
        if (i > 2) {
            EXPECT_EQ(jitter_estimator_->GetJitterEstimate(0, std::nullopt, true /* enble_reduced_delay */), 0);
        }
        gen.Advance();
    }
}

TEST_F(JitterEstimatorTest, TestLowFrameRateDisabled) {
    SampleGenerator gen(10);
    int fps = 5;
    int64_t frame_delta_us = kNumMicrosecsPerSec / fps;
    for (int i = 0; i < 60; ++i) {
        jitter_estimator_->UpdateEstimate(gen.Delay(), gen.FrameSize());
        fake_clock_->AdvanceTimeUs(frame_delta_us);
        if (i > 2) {
            EXPECT_GT(jitter_estimator_->GetJitterEstimate(0, std::nullopt, false /* enble_reduced_delay */), 0);
        }
        gen.Advance();
    }
}

TEST_F(JitterEstimatorTest, TestUpperBound) {
    struct TestContext {
        TestContext() 
            : upper_bound(0.0),
              rtt_mult(0),
              rtt_mult_add_cap_ms(std::nullopt),
              percentile_counter(1000) {}
        double upper_bound;
        double rtt_mult;
        std::optional<double> rtt_mult_add_cap_ms;
        HistogramPercentileCounter percentile_counter;
    };

    std::vector<TestContext> test_cases(4);

    // Large upper bound, rtt_mult = 0, and nullopt for rtt_mult addition cap.
    test_cases[0].upper_bound = 100.0;
    test_cases[0].rtt_mult = 0;
    test_cases[0].rtt_mult_add_cap_ms = std::nullopt;
    // Small upper bound, rtt_mult = 0, and nullopt for rtt_mult addition cap.
    test_cases[1].upper_bound = 3.5;
    test_cases[1].rtt_mult = 0;
    test_cases[1].rtt_mult_add_cap_ms = std::nullopt;
    // Large upper bound, rtt_mult = 1, and large rtt_mult addition cap value.
    test_cases[2].upper_bound = 1000.0;
    test_cases[2].rtt_mult = 1.0;
    test_cases[2].rtt_mult_add_cap_ms = 200.0;
    // Large upper bound, rtt_mult = 1, and small rtt_mult addition cap value.
    test_cases[3].upper_bound = 1000.0;
    test_cases[3].rtt_mult = 1.0;
    test_cases[3].rtt_mult_add_cap_ms = 10.0;

    JitterEstimator::HyperParameters hyper_params;
    for (TestContext& ctx : test_cases) {
        hyper_params.time_deviation_upper_bound = ctx.upper_bound;
        Reset(hyper_params);

        SampleGenerator gen(50);
        uint64_t time_delta_us = kNumMicrosecsPerSec / 30;
        const int64_t kRttMs = 250;
        for (int i = 0; i < 100; ++i) {
            jitter_estimator_->UpdateEstimate(gen.Delay(), gen.FrameSize());
            fake_clock_->AdvanceTimeUs(time_delta_us);
            jitter_estimator_->FrameNacked();
            jitter_estimator_->UpdateRtt(kRttMs);
            ctx.percentile_counter.Add(static_cast<uint32_t>(jitter_estimator_->GetJitterEstimate(ctx.rtt_mult, ctx.rtt_mult_add_cap_ms)));
            gen.Advance();
        }
    }

    // Median should be similar after three seconds. Allow 5% error margin.
    uint32_t median_unbound = *test_cases[0].percentile_counter.GetPercentile(0.5);
    uint32_t median_bounded = *test_cases[1].percentile_counter.GetPercentile(0.5);
    EXPECT_NEAR(median_unbound, median_bounded, (median_unbound * 5) / 100);
    // Max should be lower for the bounded case.
    uint32_t max_unbound = *test_cases[0].percentile_counter.GetPercentile(1.0);
    uint32_t max_bounded = *test_cases[1].percentile_counter.GetPercentile(1.0);
    EXPECT_GT(max_unbound, static_cast<uint32_t>(max_bounded * 1.25));
    // With rtt_mult = 1, max should be lower with small rtt_mult add cap value.
    max_unbound = *test_cases[2].percentile_counter.GetPercentile(1.0);
    max_bounded = *test_cases[3].percentile_counter.GetPercentile(1.0);
    EXPECT_GT(max_unbound, static_cast<uint32_t>(max_bounded * 1.25));
}
    
} // namespace test   
} // namespace naivertc
