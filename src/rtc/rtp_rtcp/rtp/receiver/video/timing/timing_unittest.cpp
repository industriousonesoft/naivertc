#include "rtc/rtp_rtcp/rtp/receiver/video/timing/timing.hpp"
#include "rtc/base/time/clock_simulated.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

namespace naivertc {
namespace test {
namespace {

constexpr int kFps = 25;
constexpr int kDefaultRenderDelayMs = 10;
constexpr int kDelayMaxChangeMsPerS = 100;

}  // namespace

class T(ReceiverTimingTest) : public ::testing::Test {
public:
    T(ReceiverTimingTest)() 
        : clock_(std::make_shared<SimulatedClock>(0)),
          timing_(clock_) {};
protected:
    std::shared_ptr<SimulatedClock> clock_;
    rtp::video::Timing timing_;
};

MY_TEST_F(ReceiverTimingTest, JitterDelay) {
    timing_.Reset();

    uint32_t timestamp = 0;
    timing_.UpdateCurrentDelay(timestamp);

    timing_.Reset();

    timing_.IncomingTimestamp(timestamp, clock_->now_ms());
    uint32_t jitter_delay_ms = 20;
    timing_.set_jitter_delay_ms(jitter_delay_ms);
    timing_.UpdateCurrentDelay(timestamp);
    timing_.set_render_delay_ms(0);
    int64_t render_time_ms = timing_.RenderTimeMs(timestamp, clock_->now_ms());
    uint32_t wait_time_ms = timing_.MaxWaitingTimeBeforeDecode(render_time_ms, clock_->now_ms());
    // Since we have no decode delay we get `wait_time_ms` = RenderTime - now - renderDelay = (now + jitter) - now - 0 = jitter.
    EXPECT_EQ(wait_time_ms, jitter_delay_ms);

    jitter_delay_ms += kDelayMaxChangeMsPerS + 10;
    timestamp += 90000; // Step forward 1s.
    clock_->AdvanceTimeMs(1000);
    timing_.set_jitter_delay_ms(jitter_delay_ms);
    timing_.UpdateCurrentDelay(timestamp);
    wait_time_ms = timing_.MaxWaitingTimeBeforeDecode(timing_.RenderTimeMs(timestamp, clock_->now_ms()), clock_->now_ms());
    // Since we gradually increase the delay we only get 100 ms every second.
    EXPECT_EQ(jitter_delay_ms - 10, wait_time_ms);

    timestamp += 90000;
    clock_->AdvanceTimeMs(1000);
    timing_.UpdateCurrentDelay(timestamp);
    wait_time_ms = timing_.MaxWaitingTimeBeforeDecode(timing_.RenderTimeMs(timestamp, clock_->now_ms()), clock_->now_ms());
    EXPECT_EQ(wait_time_ms, jitter_delay_ms);

    // Insert frames without jitter, verify that this gives the exact wait time.
    const int kNumFrames = 300;
    for (int i = 0; i < kNumFrames; ++i) {
        clock_->AdvanceTimeMs(1000 / kFps);
        // The timestamp interval between two frames.
        timestamp += 90000 / kFps;
        timing_.IncomingTimestamp(timestamp, clock_->now_ms());
    }
    timing_.UpdateCurrentDelay(timestamp);
    wait_time_ms = timing_.MaxWaitingTimeBeforeDecode(timing_.RenderTimeMs(timestamp, clock_->now_ms()), clock_->now_ms());
    EXPECT_EQ(jitter_delay_ms, wait_time_ms);

    // Add decode time estimates for 1 second.
    const uint32_t kDecodeTimeMs = 10;
    for (int i = 0; i < kFps; ++i) {
        clock_->AdvanceTimeMs(kDecodeTimeMs);
        timing_.AddDecodeTime(kDecodeTimeMs, clock_->now_ms());
        timestamp += 90000 / kFps;
        // Insert new frame
        clock_->AdvanceTimeMs(1000 / kFps - kDecodeTimeMs);
        timing_.IncomingTimestamp(timestamp, clock_->now_ms());
    }
    timing_.UpdateCurrentDelay(timestamp);
    wait_time_ms = timing_.MaxWaitingTimeBeforeDecode(timing_.RenderTimeMs(timestamp, clock_->now_ms()), clock_->now_ms());
    EXPECT_EQ(jitter_delay_ms, wait_time_ms);

    const int kMinTotalDelayMs = 200;
    timing_.set_min_playout_delay_ms(kMinTotalDelayMs);
    clock_->AdvanceTimeMs(5000);
    timestamp += 5 * 90000;
    timing_.UpdateCurrentDelay(timestamp);
    const int kRenderDelayMs = 10;
    timing_.set_render_delay_ms(kRenderDelayMs);
    wait_time_ms = timing_.MaxWaitingTimeBeforeDecode(timing_.RenderTimeMs(timestamp, clock_->now_ms()), clock_->now_ms());
    // We should at least have kMinTotalDelayMs - kDecodeTimeMs - kRenderDelayMs to wait.
    EXPECT_EQ(kMinTotalDelayMs - kDecodeTimeMs - kRenderDelayMs, wait_time_ms);
    // The total video delay should be equal to the min total delay.
    EXPECT_EQ(kMinTotalDelayMs, timing_.TargetDelayMs());

    // Reset playout delay.
    timing_.set_min_playout_delay_ms(0);
    clock_->AdvanceTimeMs(5000);
    timestamp += 5 * 90000;
    timing_.UpdateCurrentDelay(timestamp);
}

MY_TEST_F(ReceiverTimingTest, TimestampWrapAround) {
    timing_.Reset();
    uint32_t ts_interval = 90'000 / kFps;
    int time_interval = 1'000 / kFps;
    // Provoke a wrap-around. The fifth frame will have wrapped at 25 fps.
    uint32_t timestamp = 0xFFFFFFFFu - 3 * ts_interval;
    for (int i = 0; i < 5; ++i) {
        timing_.IncomingTimestamp(timestamp, clock_->now_ms());
        clock_->AdvanceTimeMs(time_interval);
        timestamp += ts_interval;
        EXPECT_EQ(3 * time_interval, timing_.RenderTimeMs(0xFFFFFFFFu, clock_->now_ms()));
        EXPECT_EQ(3 * time_interval + 1, timing_.RenderTimeMs(89u /* 1 ms later in 90 kHz */, clock_->now_ms()));
        EXPECT_EQ(3 * time_interval + 1, timing_.RenderTimeMs(90u, clock_->now_ms()));
        EXPECT_EQ(3 * time_interval + 2, timing_.RenderTimeMs(180u, clock_->now_ms()));
    }
}

MY_TEST_F(ReceiverTimingTest, MaxWaitingTimeBeforeDecodeIsZeroForZeroRenderTime) {
    // This is the default path when the RTP playout delay header extension is set
    // to min==0.
    constexpr int64_t kTimeDeltaMs = 1000.0 / 60.0;
    constexpr int64_t kZeroRenderTimeMs = 0;
    timing_.Reset();
    for (int i = 0; i < 10; ++i) {
        clock_->AdvanceTimeMs(kTimeDeltaMs);
        EXPECT_LT(timing_.MaxWaitingTimeBeforeDecode(kZeroRenderTimeMs, clock_->now_ms()), 0);
    }
    
    // Another frame submitted at the same time also returns a negative max
    // waiting time.
    int64_t now_ms = clock_->now_ms();
    EXPECT_LT(timing_.MaxWaitingTimeBeforeDecode(kZeroRenderTimeMs, now_ms), 0);
    // MaxWaitingTimeBeforeDecode should be less than zero even if there's a burst of frames.
    EXPECT_LT(timing_.MaxWaitingTimeBeforeDecode(kZeroRenderTimeMs, now_ms), 0);
    EXPECT_LT(timing_.MaxWaitingTimeBeforeDecode(kZeroRenderTimeMs, now_ms), 0);
    EXPECT_LT(timing_.MaxWaitingTimeBeforeDecode(kZeroRenderTimeMs, now_ms), 0);
}

MY_TEST_F(ReceiverTimingTest, MaxWaitingTimeBeforeDecodeZeroDelayPacingExperiment) {
    // The minimum pacing is enabled by a field trial and active if the RTP
    // playout delay header extension is set to min==0.
    constexpr int64_t kMinPacingMs = 3;
    constexpr int64_t kTimeDeltaMs = 1000.0 / 60.0;
    constexpr int64_t kZeroRenderTimeMs = 0;

    timing_.Reset();
    timing_.set_zero_playout_delay_min_pacing(TimeDelta::Millis(3));
    // MaxWaitingTimeBeforeDecode() returns zero for evenly spaced video frames.
    for (int i = 0; i < 10; ++i) {
        clock_->AdvanceTimeMs(kTimeDeltaMs);
        int64_t now_ms = clock_->now_ms();
        EXPECT_EQ(timing_.MaxWaitingTimeBeforeDecode(kZeroRenderTimeMs, now_ms), 0);
    }
    // Another frame submitted at the same time is paced according to the field
    // trial setting.
    int64_t now_ms = clock_->now_ms();
    EXPECT_EQ(timing_.MaxWaitingTimeBeforeDecode(kZeroRenderTimeMs, now_ms), kMinPacingMs);
    // If there's a burst of frames, the min pacing interval is summed.
    EXPECT_EQ(timing_.MaxWaitingTimeBeforeDecode(kZeroRenderTimeMs, now_ms), 2 * kMinPacingMs);
    EXPECT_EQ(timing_.MaxWaitingTimeBeforeDecode(kZeroRenderTimeMs, now_ms), 3 * kMinPacingMs);
    EXPECT_EQ(timing_.MaxWaitingTimeBeforeDecode(kZeroRenderTimeMs, now_ms), 4 * kMinPacingMs);
    // Allow a few ms to pass, this should be subtracted from the MaxWaitingTimeBeforeDecode.
    constexpr int64_t kTwoMs = 2;
    clock_->AdvanceTimeMs(kTwoMs);
    now_ms = clock_->now_ms();
    EXPECT_EQ(timing_.MaxWaitingTimeBeforeDecode(kZeroRenderTimeMs, now_ms), 5 * kMinPacingMs - kTwoMs);
}

MY_TEST_F(ReceiverTimingTest, DefaultMaxWaitingTimeBeforeDecodeUnaffectedByPacingExperiment) {
    // The minimum pacing is enabled by a field trial but should not have any
    // effect if render_time_ms is greater than 0;
    constexpr int64_t kTimeDeltaMs = 1000.0 / 60.0;
    
    timing_.Reset();
    timing_.set_zero_playout_delay_min_pacing(TimeDelta::Millis(3));

    clock_->AdvanceTimeMs(kTimeDeltaMs);
    int64_t now_ms = clock_->now_ms();
    int64_t render_time_ms = now_ms + 30;
    // Estimate the internal processing delay from the first frame.
    int64_t estimated_processing_delay = (render_time_ms - now_ms) - timing_.MaxWaitingTimeBeforeDecode(render_time_ms, now_ms);
    EXPECT_GT(estimated_processing_delay, 0);

    // Any other frame submitted at the same time should be scheduled according to
    // its render time.
    for (int i = 0; i < 5; ++i) {
        render_time_ms += kTimeDeltaMs;
        EXPECT_EQ(timing_.MaxWaitingTimeBeforeDecode(render_time_ms, now_ms), render_time_ms - now_ms - estimated_processing_delay);
    }
}
    
} // namespace test
} // namespace naivertc