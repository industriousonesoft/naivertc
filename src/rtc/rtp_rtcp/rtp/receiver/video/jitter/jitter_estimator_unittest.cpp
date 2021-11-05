#include "rtc/rtp_rtcp/rtp/receiver/video/jitter/jitter_estimator.hpp"
#include "rtc/base/time/clock_simulated.hpp"
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
          jitter_estimator_(std::make_unique<JitterEstimator>(fake_clock_)) {}
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
            EXPECT_EQ(jitter_estimator_->GetJitterEstimate(0, std::nullopt), 0);
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
            EXPECT_GT(jitter_estimator_->GetJitterEstimate(0, std::nullopt, false), 0);
        }
        gen.Advance();
    }
}
    
} // namespace test   
} // namespace naivertc
