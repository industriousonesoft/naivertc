#include "rtc/congestion_control/controllers/goog_cc/congestion_window_pushback_controller.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

class T(CongestionWindowPushbackControllerTest) : public ::testing::Test {
public:
    T(CongestionWindowPushbackControllerTest)() 
        : cwnd_controller_(nullptr) {}

    void SetUp() override {
        Reset(CongestionWindwoPushbackController::Configuration());
    }

    void Reset(CongestionWindwoPushbackController::Configuration config) {
        cwnd_controller_ = std::make_unique<CongestionWindwoPushbackController>(std::move(config));
    }

protected:
    std::unique_ptr<CongestionWindwoPushbackController> cwnd_controller_;
};

MY_TEST_F(CongestionWindowPushbackControllerTest, FullCongestionWindow) {
    // The congestion window is filling up: fill_ratio > 1.5
    cwnd_controller_->OnInflightBytes(100'000);
    cwnd_controller_->set_congestion_window(5000);

    // Decrease the taraget bitrate by 10%
    const auto target_bitrate = DataRate::BitsPerSec(80000);
    auto pushback_bitrate = cwnd_controller_->AdjustTargetBitrate(target_bitrate);
    EXPECT_EQ(pushback_bitrate, target_bitrate * 0.9);

    // Decrease the taraget bitrate by 10%
    pushback_bitrate = cwnd_controller_->AdjustTargetBitrate(target_bitrate);
    EXPECT_EQ(pushback_bitrate, target_bitrate * 0.9 * 0.9);
}

MY_TEST_F(CongestionWindowPushbackControllerTest, NormalCongestionWindow) {
    // fill_ratio > 0.1 && fill_ratio < 1.0
    cwnd_controller_->OnInflightBytes(199999);
    cwnd_controller_->set_congestion_window(200000);

    const auto target_bitrate = DataRate::BitsPerSec(80000);
    auto pushback_bitrate = cwnd_controller_->AdjustTargetBitrate(target_bitrate);
    EXPECT_EQ(target_bitrate, pushback_bitrate);
}

MY_TEST_F(CongestionWindowPushbackControllerTest, MinPushbackBitrate) {
    // The congestion window is filling up.
    cwnd_controller_->OnInflightBytes(100000);
    cwnd_controller_->set_congestion_window(50000);

    // Decrease the taraget bitrate by 10%
    const auto target_bitrate = DataRate::BitsPerSec(35000);
    auto pushback_bitrate = cwnd_controller_->AdjustTargetBitrate(target_bitrate);
    EXPECT_EQ(target_bitrate * 0.9, pushback_bitrate);

    // Decrease the taraget bitrate by 10% but must > min_pushback_bitrate (30kbps)
    cwnd_controller_->set_congestion_window(20000);
    pushback_bitrate = cwnd_controller_->AdjustTargetBitrate(target_bitrate);
    EXPECT_EQ(DataRate::BitsPerSec(30000), pushback_bitrate);
}

MY_TEST_F(CongestionWindowPushbackControllerTest, NoPushbackOnDataWindowUnset) {
    cwnd_controller_->OnInflightBytes(1e8);  // Large number

    const auto target_bitrate = DataRate::BitsPerSec(80000);
    auto pushback_bitrate = cwnd_controller_->AdjustTargetBitrate(target_bitrate);
    EXPECT_EQ(target_bitrate, pushback_bitrate);
}


} // namespace test
} // namespace naivertc