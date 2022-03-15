#include "rtc/congestion_control/send_side/goog_cc/send_side_bwe.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

void ProbingInStartPhase(bool use_delay_based) {
    SendSideBwe bwe({});
    Timestamp at_time = Timestamp::Millis(0);
    bwe.SetMinMaxBitrate(DataRate::BitsPerSec(100'000), DataRate::BitsPerSec(1500'000));
    bwe.OnSendBitrate(DataRate::BitsPerSec(200'000), at_time);

    const DataRate kInitialBitrate = DataRate::BitsPerSec(1000'000);
    const DataRate kSecondBitrate = kInitialBitrate + DataRate::BitsPerSec(500'000);

    bwe.OnPacketsLostReport(/*packets_lost=*/0, /*num_packets=*/1, at_time);
    bwe.OnRtt(TimeDelta::Millis(50), at_time);

    // The initial REMB applies immediately/
    if (use_delay_based) {
        bwe.OnDelayBasedBitrate(kInitialBitrate, at_time);
    } else {
        bwe.OnRemb(kInitialBitrate, at_time);
    }
    bwe.UpdateEstimate(at_time);
    EXPECT_EQ(kInitialBitrate, bwe.target_bitate());

    // The second REMB doesn't apply immediately.
    // Pass the start phase (2s)
    at_time += TimeDelta::Millis(2001);
    if (use_delay_based) {
        bwe.OnDelayBasedBitrate(kSecondBitrate, at_time);
    } else {
        bwe.OnRemb(kSecondBitrate, at_time);
    }
    bwe.UpdateEstimate(at_time);
    EXPECT_EQ(kInitialBitrate, bwe.target_bitate());

}

MY_TEST(SendSideBweTest, InitialRembWithProbing) {
    ProbingInStartPhase(false);
}

MY_TEST(SendSideBWeTest, InitialDelayBasedBweWithProbing) {
    ProbingInStartPhase(true);
}

MY_TEST(SendSideBweTest, DosentReapplyBitrateDecreaseWithoutFollowingRemb) {
    SendSideBwe bwe({});
    const DataRate kMinBitrate = DataRate::BitsPerSec(100'000);
    const DataRate kInitialBitrate = DataRate::BitsPerSec(1000'000);
    Timestamp at_time = Timestamp::Millis(0);
    bwe.SetMinMaxBitrate(kMinBitrate, DataRate::BitsPerSec(1500'000));
    bwe.OnSendBitrate(kInitialBitrate, at_time);

    // Equalt to 50% in ratio.
    const uint8_t kFractionLoss = 128;
    const TimeDelta kRtt = TimeDelta::Millis(50);
    at_time += TimeDelta::Millis(10'000);

    EXPECT_EQ(kInitialBitrate, bwe.target_bitate());
    EXPECT_EQ(0, bwe.fraction_loss());
    EXPECT_EQ(0, bwe.rtt().ms());

    // Signal heavy loss to go down in bitrate.
    bwe.OnPacketsLostReport(/*packets_lost=*/50, /*num_packets=*/100, at_time);
    bwe.OnRtt(kRtt, at_time);

    // Triger an update 2 seconds later to not be rate limited.
    at_time += TimeDelta::Millis(1000);
    bwe.UpdateEstimate(at_time);
    EXPECT_LT(bwe.target_bitate(), kInitialBitrate);
    // Verify that the threhold bitrate isn't hitting the min bitrate.
    // If this ever happens, update the thresholds or loss rate so than
    // it doesn't hit min bitrate after ont bitrate update.
    EXPECT_GT(bwe.target_bitate(), kMinBitrate);
    EXPECT_EQ(kFractionLoss, bwe.fraction_loss());
    EXPECT_EQ(kRtt, bwe.rtt());

    // Triggering an update shouldn't apply further downgrage nor upgrade
    // since there's no intermediate receiver block received indicating whether
    // this is currently good or not.
    DataRate last_updated_bitrate = bwe.target_bitate();
    // Trigger an update 2 seconds later to not be rate limited, but it still 
    // shouldn't update.
    at_time += TimeDelta::Millis(1000);
    bwe.UpdateEstimate(at_time);

    EXPECT_EQ(last_updated_bitrate, bwe.target_bitate());
    // The old loss rate and RTT should still be applied though.
    EXPECT_EQ(kFractionLoss, bwe.fraction_loss());
    EXPECT_EQ(kRtt, bwe.rtt());
}

MY_TEST(SendSideBweTest, SettingSendBitrateOverideDelayBasedEstimate) {
    const DataRate kMinBitrate = DataRate::BitsPerSec(10000);
    const DataRate kMaxBitrate = DataRate::BitsPerSec(10'000'000);
    const DataRate kInitialBitrate = DataRate::BitsPerSec(300'000);
    const DataRate kDelayBasedBitrate = DataRate::BitsPerSec(350'000);
    const DataRate kForcedHighBitrate = DataRate::BitsPerSec(2500'000);

    SendSideBwe bwe({});
    Timestamp at_time = Timestamp::Millis(0);

    bwe.SetMinMaxBitrate(kMinBitrate, kMaxBitrate);
    bwe.OnSendBitrate(kInitialBitrate, at_time);
    bwe.OnDelayBasedBitrate(kDelayBasedBitrate, at_time);

    bwe.UpdateEstimate(at_time);

    EXPECT_GE(bwe.target_bitate(), kInitialBitrate) << bwe.target_bitate().bps();
    EXPECT_LE(bwe.target_bitate(), kDelayBasedBitrate);

    bwe.OnSendBitrate(kForcedHighBitrate, at_time);
    EXPECT_EQ(bwe.target_bitate(), kForcedHighBitrate);
}
    
} // namespace test
} // namespace naivertc