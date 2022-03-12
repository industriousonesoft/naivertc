#include "rtc/congestion_control/pacing/bitrate_prober.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

namespace naivertc {
namespace test {
namespace {

constexpr DataRate kTestBitrate1 = DataRate::KilobitsPerSec(900);
constexpr DataRate kTestBitrate2 = DataRate::KilobitsPerSec(1800);
constexpr int kMinNumProbes = 5;
constexpr size_t kProbeSize = 1000;
constexpr TimeDelta kMinProbeDuration = TimeDelta::Millis(15);
constexpr TimeDelta kMaxProbeDelay = TimeDelta::Millis(3);
    
} // namespace


class T(BitrateProberTest) : public ::testing::Test {
public:
    T(BitrateProberTest)() 
        : clock_(1000'000) {
        prober_ = std::make_unique<BitrateProber>(BitrateProber::Configuration());
    }

    void Reset(BitrateProber::Configuration config) {
        prober_.reset(new BitrateProber(config));
    }

protected:
    SimulatedClock clock_;
    std::unique_ptr<BitrateProber> prober_;
};

MY_TEST_F(BitrateProberTest, VerifyStatesAndTimeBetweenProbes) {
    auto now = clock_.CurrentTime();
    EXPECT_EQ(prober_->NextTimeToProbe(now), Timestamp::PlusInfinity());

    // Only the call |OnIncomingPacket| can change the state to active.
    prober_->AddProbeCluster(0, kTestBitrate1, now);
    prober_->AddProbeCluster(1, kTestBitrate2, now);
    EXPECT_FALSE(prober_->IsProbing());
    // Only return a availale cluster in active state.
    EXPECT_FALSE(prober_->CurrentProbeCluster(now));

    prober_->OnIncomingPacket(kProbeSize);
    EXPECT_TRUE(prober_->IsProbing());
    EXPECT_EQ(0, prober_->CurrentProbeCluster(now)->id);

    // First packet should be probe as soon as possible.
    EXPECT_EQ(Timestamp::MinusInfinity(), prober_->NextTimeToProbe(now));

    // Send probes with kTestBitrate1.
    auto start_time = now;
    for (int i = 0; i < kMinNumProbes; ++i) {
        now = std::max(now, prober_->NextTimeToProbe(now));
        EXPECT_EQ(0, prober_->CurrentProbeCluster(now)->id);
        prober_->OnProbeSent(kProbeSize, now);
    }

    auto probe_duration = now - start_time;
    // (kProbeSize * kMinNumProbes * 8000) / kTestBitrate1 = 1000 * 5 * 8000 / 900'000 = 400 / 9 ~= 44.45 ms
    EXPECT_GE(probe_duration, kMinProbeDuration);

    // Verify that the actual bitrate is within 10% of the target.
    DataRate bitrate = kProbeSize * (kMinNumProbes - 1) / probe_duration;
    EXPECT_GT(bitrate, kTestBitrate1 * 0.9);
    EXPECT_LT(bitrate, kTestBitrate1 * 1.1);

    now = std::max(now, prober_->NextTimeToProbe(now));
    start_time = now;

    for (int i = 0; i < kMinNumProbes; ++i) {
        now = std::max(now, prober_->NextTimeToProbe(now));
        EXPECT_EQ(1, prober_->CurrentProbeCluster(now)->id);
        prober_->OnProbeSent(kProbeSize, now);
    }

    probe_duration = now - start_time;
    EXPECT_GE(probe_duration, kMinProbeDuration);
    bitrate = kProbeSize * (kMinNumProbes - 1) / probe_duration;
    EXPECT_GT(bitrate, kTestBitrate2 * 0.9);
    EXPECT_LT(bitrate, kTestBitrate2 * 1.1);

    EXPECT_EQ(Timestamp::PlusInfinity(), prober_->NextTimeToProbe(now));
    EXPECT_FALSE(prober_->IsProbing());

}

MY_TEST_F(BitrateProberTest, DoesntProbeWithoutRecentPackets) {
    Timestamp now = clock_.CurrentTime();
    EXPECT_EQ(prober_->NextTimeToProbe(now), Timestamp::PlusInfinity());

    prober_->AddProbeCluster(0, kTestBitrate1, now);
    EXPECT_FALSE(prober_->IsProbing());

    prober_->OnIncomingPacket(kProbeSize);
    EXPECT_TRUE(prober_->IsProbing());
    EXPECT_EQ(now, std::max(now, prober_->NextTimeToProbe(now)));
    prober_->OnProbeSent(kProbeSize, now);
}

MY_TEST_F(BitrateProberTest, DoesntDiscardDelayedProbesInLegacyMode) {
    BitrateProber::Configuration config;
    config.abort_delayed_probes = false;
    config.max_probe_delay = kMaxProbeDelay;
    Reset(config);

    Timestamp now = clock_.CurrentTime();
    prober_->AddProbeCluster(0, kTestBitrate1, now);
    prober_->OnIncomingPacket(kProbeSize);
    EXPECT_TRUE(prober_->IsProbing());
    EXPECT_EQ(prober_->CurrentProbeCluster(now)->id, 0);
    // Advance to first probe time and indicate sent probe.
    now = std::max(now, prober_->NextTimeToProbe(now));
    prober_->OnProbeSent(kProbeSize, now);

    auto next_time_to_probe = prober_->NextTimeToProbe(now);
    auto delta = next_time_to_probe - now;
    EXPECT_GT(delta, TimeDelta::Zero());
    // Advance time 1ms past timeout for the next probe.
    clock_.AdvanceTime(delta + kMaxProbeDelay + TimeDelta::Millis(1));
    now = clock_.CurrentTime();

    EXPECT_EQ(prober_->NextTimeToProbe(now), Timestamp::PlusInfinity());
    // Check that legacy behaviour where prober is reset in TimeUntilNextProbe is
    // no longer there. Probes are no longer retried if they are timed out.
    prober_->OnIncomingPacket(kProbeSize);
    EXPECT_EQ(prober_->NextTimeToProbe(now), Timestamp::PlusInfinity());
}

MY_TEST_F(BitrateProberTest, DiscardDelayedProbesInLegacyMode) {
    BitrateProber::Configuration config;
    config.abort_delayed_probes = true;
    config.max_probe_delay = kMaxProbeDelay;
    Reset(config);

    Timestamp now = clock_.CurrentTime();
    prober_->AddProbeCluster(0, kTestBitrate1, now);
    prober_->OnIncomingPacket(kProbeSize);
    EXPECT_TRUE(prober_->IsProbing());
    EXPECT_EQ(prober_->CurrentProbeCluster(now)->id, 0);
    // Advance to first probe time and indicate sent probe.
    now = std::max(now, prober_->NextTimeToProbe(now));
    prober_->OnProbeSent(kProbeSize, now);

    auto next_time_to_probe = prober_->NextTimeToProbe(now);
    auto delta = next_time_to_probe - now;
    EXPECT_GT(delta, TimeDelta::Zero());
    // Advance time 1ms past timeout for the next probe.
    clock_.AdvanceTime(delta + kMaxProbeDelay + TimeDelta::Millis(1));
    now = clock_.CurrentTime();

    // Still indicates the time we wanted to probe at.
    EXPECT_EQ(prober_->NextTimeToProbe(now), next_time_to_probe);
    // First and only cluster removed due to timeout.
    EXPECT_FALSE(prober_->CurrentProbeCluster(now).has_value());
}

MY_TEST_F(BitrateProberTest, DoesntInitializeProbingForSmallPackets) {
    prober_->SetEnabled(true);
    EXPECT_FALSE(prober_->IsProbing());

    prober_->OnIncomingPacket(100);
    EXPECT_FALSE(prober_->IsProbing());
}

MY_TEST_F(BitrateProberTest, VerifyProbeSizeOnHighBitrate) {
    const DataRate kHighBitrate = DataRate::KilobitsPerSec(10000);  // 10 Mbps

    prober_->AddProbeCluster(/*cluster_id=*/0, kHighBitrate, clock_.CurrentTime());
    // Probe size should ensure a minimum of 1 ms interval.
    EXPECT_GT(prober_->RecommendedMinProbeSize(),
              kHighBitrate * TimeDelta::Millis(1));
}

MY_TEST_F(BitrateProberTest, MinumumNumberOfProbingPackets) {
    // Even when probing at a low bitrate we expect a minimum number
    // of packets to be sent.
    const DataRate kBitrate = DataRate::KilobitsPerSec(100);
  
    Timestamp now = clock_.CurrentTime();
    prober_->AddProbeCluster(0, kBitrate, now);
    prober_->OnIncomingPacket(kProbeSize);
    for (int i = 0; i < kMinNumProbes; ++i) {
        EXPECT_TRUE(prober_->IsProbing());
        prober_->OnProbeSent(kProbeSize, now);
    }
    // The state has switched from active to suspended.
    EXPECT_FALSE(prober_->IsProbing());
}

MY_TEST_F(BitrateProberTest, ScaleBytesUsedForProbing) {
    const DataRate kBitrate = DataRate::KilobitsPerSec(10000);  // 10 Mbps.
    const size_t kExpectedBytesSent = kBitrate * kMinProbeDuration;

    Timestamp now = clock_.CurrentTime();
    prober_->AddProbeCluster(/*cluster_id=*/0, kBitrate, now);
    prober_->OnIncomingPacket(kProbeSize);
    size_t sent_bytes = 0;
    while (sent_bytes < kExpectedBytesSent) {
        ASSERT_TRUE(prober_->IsProbing());
        prober_->OnProbeSent(kProbeSize, now);
        sent_bytes += kProbeSize;
    }
    EXPECT_FALSE(prober_->IsProbing());
}

MY_TEST_F(BitrateProberTest, HighBitrateProbing) {
    const DataRate kBitrate = DataRate::KilobitsPerSec(1000000);  // 1 Gbps.
    const size_t kExpectedBytesSent = kBitrate * kMinProbeDuration;

    Timestamp now = clock_.CurrentTime();
    prober_->AddProbeCluster(0, kBitrate, now);
    prober_->OnIncomingPacket(kProbeSize);
    size_t sent_bytes = 0;
    while (sent_bytes < kExpectedBytesSent) {
        ASSERT_TRUE(prober_->IsProbing());
        prober_->OnProbeSent(kProbeSize, now);
        sent_bytes += kProbeSize;
    }
    EXPECT_FALSE(prober_->IsProbing());
}

MY_TEST_F(BitrateProberTest, ProbeClusterTimeout) {

    const DataRate kBitrate = DataRate::KilobitsPerSec(300);
    const size_t kSmallPacketSize = 20;
    // Expecting two probe clusters of 5 packets each.
    const size_t kExpectedBytesSent = kSmallPacketSize * 2 * 5;
    const TimeDelta kTimeout = TimeDelta::Seconds(5); // 5s

    prober_->AddProbeCluster(/*cluster_id=*/0, kBitrate, clock_.CurrentTime());
    prober_->OnIncomingPacket(kSmallPacketSize);
    EXPECT_FALSE(prober_->IsProbing());
    
    // The cluster 1 is still in the cluster queue.
    clock_.AdvanceTime(kTimeout);

    prober_->AddProbeCluster(/*cluster_id=*/1, kBitrate / 10, clock_.CurrentTime());
    prober_->OnIncomingPacket(kSmallPacketSize);
    EXPECT_FALSE(prober_->IsProbing());

    // The cluster 1 was removed as timed out, so the min recommended size works.
    clock_.AdvanceTimeMs(1);

    prober_->AddProbeCluster(/*cluster_id=*/2, kBitrate / 10, clock_.CurrentTime());
    prober_->OnIncomingPacket(kSmallPacketSize);
    EXPECT_TRUE(prober_->IsProbing());

    auto now = clock_.CurrentTime();
    size_t sent_bytes = 0;
    while (sent_bytes < kExpectedBytesSent) {
        ASSERT_TRUE(prober_->IsProbing());
        prober_->OnProbeSent(kSmallPacketSize, now);
        sent_bytes += kSmallPacketSize;
    }

    EXPECT_FALSE(prober_->IsProbing());
}
    
} // namespace test
} // namespace naivertc
