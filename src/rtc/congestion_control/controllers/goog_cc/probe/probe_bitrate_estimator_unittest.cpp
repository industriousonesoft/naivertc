#include "rtc/congestion_control/controllers/goog_cc/probe/probe_bitrate_estimator.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

#include <optional>

namespace naivertc {
namespace test {
namespace {
    
constexpr int kDefaultMinProbes = 5;
constexpr size_t kDefaultMinBytes = 5000;
constexpr float kTargetUtilizationFraction = 0.95f;

} // namespace

class T(ProbeBitrateEstimatorTest) : public ::testing::Test {
public:
    T(ProbeBitrateEstimatorTest)() 
        : probe_bitrate_estimator_(),
          measured_bitrate_(std::nullopt) {}

    void AddPacketFeedback(int probe_cluster_id,
                           size_t size_bytes,
                           int64_t send_time_ms,
                           int64_t recv_time_ms,
                           int min_probes = kDefaultMinProbes,
                           size_t min_bytes = kDefaultMinBytes) {
        const Timestamp kReferenceTime = Timestamp::Seconds(1000);
        PacketResult feedback;
        feedback.sent_packet.send_time = kReferenceTime + TimeDelta::Millis(send_time_ms);
        feedback.sent_packet.size = size_bytes;
        // ProbeCluster
        ProbeCluster probe_cluster = {probe_cluster_id, min_probes, min_bytes, DataRate::Zero()};
        // PacedPacketInfo
        feedback.sent_packet.pacing_info.probe_cluster.emplace(std::move(probe_cluster));
        feedback.recv_time = kReferenceTime + TimeDelta::Millis(recv_time_ms);
        measured_bitrate_ = probe_bitrate_estimator_.IncomingProbePacketFeedback(feedback);
    }

protected:
    ProbeBitrateEstimator probe_bitrate_estimator_;
    std::optional<DataRate> measured_bitrate_;
};

MY_TEST_F(ProbeBitrateEstimatorTest, OneCluster) {
    // One cluster consisting of 4 probes and
    // fit to 90% of kDefaultMinProbes (=5)
    AddPacketFeedback(0, 1000, 0, 10);
    AddPacketFeedback(0, 1000, 10, 20);
    AddPacketFeedback(0, 1000, 20, 30);
    AddPacketFeedback(0, 1000, 30, 40);

    EXPECT_TRUE(measured_bitrate_.has_value());
    EXPECT_NEAR(measured_bitrate_->bps(), 800000, 10);
}

MY_TEST_F(ProbeBitrateEstimatorTest, OneClusterTooFewProbes) {
    AddPacketFeedback(0, 1000, 0, 10);
    AddPacketFeedback(0, 1000, 10, 20);
    AddPacketFeedback(0, 1000, 20, 30);

    EXPECT_FALSE(measured_bitrate_.has_value());
}

MY_TEST_F(ProbeBitrateEstimatorTest, OneClusterTooFewBytes) {
    const int kMinBytes = 6000;
    AddPacketFeedback(0, 800, 0, 10, kDefaultMinProbes, kMinBytes);
    AddPacketFeedback(0, 800, 10, 20, kDefaultMinProbes, kMinBytes);
    AddPacketFeedback(0, 800, 20, 30, kDefaultMinProbes, kMinBytes);
    AddPacketFeedback(0, 800, 30, 40, kDefaultMinProbes, kMinBytes);
    AddPacketFeedback(0, 800, 40, 50, kDefaultMinProbes, kMinBytes);

    EXPECT_FALSE(measured_bitrate_.has_value());
}

MY_TEST_F(ProbeBitrateEstimatorTest, SmallCluster) {
    const int kMinBytes = 1000;
    AddPacketFeedback(0, 150, 0, 10, kDefaultMinProbes, kMinBytes);
    AddPacketFeedback(0, 150, 10, 20, kDefaultMinProbes, kMinBytes);
    AddPacketFeedback(0, 150, 20, 30, kDefaultMinProbes, kMinBytes);
    AddPacketFeedback(0, 150, 30, 40, kDefaultMinProbes, kMinBytes);
    AddPacketFeedback(0, 150, 40, 50, kDefaultMinProbes, kMinBytes);
    AddPacketFeedback(0, 150, 50, 60, kDefaultMinProbes, kMinBytes);
    EXPECT_NEAR(measured_bitrate_->bps(), 120000, 10);
}

MY_TEST_F(ProbeBitrateEstimatorTest, LargeCluster) {
    const int kMinProbes = 30;
    const int kMinBytes = 312500;

    int64_t send_time = 0;
    int64_t receive_time = 5;
    for (int i = 0; i < 25; ++i) {
        AddPacketFeedback(0, 12500, send_time, receive_time, kMinProbes, kMinBytes);
        ++send_time;
        ++receive_time;
    }
    EXPECT_NEAR(measured_bitrate_->bps(), 100000000, 10);
}

MY_TEST_F(ProbeBitrateEstimatorTest, TooFastReceive) {
    AddPacketFeedback(0, 1000, 0, 19);
    AddPacketFeedback(0, 1000, 10, 22);
    AddPacketFeedback(0, 1000, 20, 25);
    AddPacketFeedback(0, 1000, 40, 27);

    EXPECT_FALSE(measured_bitrate_.has_value());
}

MY_TEST_F(ProbeBitrateEstimatorTest, SlowReceive) {
    AddPacketFeedback(0, 1000, 0, 10);
    AddPacketFeedback(0, 1000, 10, 40);
    AddPacketFeedback(0, 1000, 20, 70);
    AddPacketFeedback(0, 1000, 30, 85);
    // Expected send rate = (4000 - 1000 /* last send packet */) * 8000 / (30 - 0) = 800 kbps
    // Expected receive rate = (4000 - 1000 /* first receive packet */) * 8000 / (85 - 10) = 320 kbps.
    EXPECT_NEAR(measured_bitrate_->bps(), kTargetUtilizationFraction * 320000,
                10);
}

MY_TEST_F(ProbeBitrateEstimatorTest, BurstReceive) {
    AddPacketFeedback(0, 1000, 0, 50);
    AddPacketFeedback(0, 1000, 10, 50);
    AddPacketFeedback(0, 1000, 20, 50);
    AddPacketFeedback(0, 1000, 40, 50);

    EXPECT_FALSE(measured_bitrate_.has_value());
}

MY_TEST_F(ProbeBitrateEstimatorTest, MultipleClusters) {
    AddPacketFeedback(0, 1000, 0, 10);
    AddPacketFeedback(0, 1000, 10, 20);
    AddPacketFeedback(0, 1000, 20, 30);
    AddPacketFeedback(0, 1000, 40, 60);
    // Expected send rate = 600 kbps, expected receive rate = 480 kbps.
    EXPECT_NEAR(measured_bitrate_->bps(), kTargetUtilizationFraction * 480000,
                10);

    AddPacketFeedback(0, 1000, 50, 60);
    // Expected send rate = 640 kbps, expected receive rate = 640 kbps.
    EXPECT_NEAR(measured_bitrate_->bps(), 640000, 10);

    AddPacketFeedback(1, 1000, 60, 70);
    AddPacketFeedback(1, 1000, 65, 77);
    AddPacketFeedback(1, 1000, 70, 84);
    AddPacketFeedback(1, 1000, 75, 90);
    // Expected send rate = 1600 kbps, expected receive rate = 1200 kbps.

    EXPECT_NEAR(measured_bitrate_->bps(), kTargetUtilizationFraction * 1200000,
                10);
}

MY_TEST_F(ProbeBitrateEstimatorTest, IgnoreOldClusters) {
    AddPacketFeedback(0, 1000, 0, 10);
    AddPacketFeedback(0, 1000, 10, 20);
    AddPacketFeedback(0, 1000, 20, 30);

    AddPacketFeedback(1, 1000, 60, 70);
    AddPacketFeedback(1, 1000, 65, 77);
    AddPacketFeedback(1, 1000, 70, 84);
    AddPacketFeedback(1, 1000, 75, 90);
    // Expected send rate = 1600 kbps, expected receive rate = 1200 kbps.

    EXPECT_NEAR(measured_bitrate_->bps(), kTargetUtilizationFraction * 1200000,
                10);

    // Coming in 6s later
    AddPacketFeedback(0, 1000, 40 + 6000, 60 + 6000);

    EXPECT_FALSE(measured_bitrate_.has_value());
}

MY_TEST_F(ProbeBitrateEstimatorTest, IgnoreSizeLastSendPacket) {
    AddPacketFeedback(0, 1000, 0, 10);
    AddPacketFeedback(0, 1000, 10, 20);
    AddPacketFeedback(0, 1000, 20, 30);
    AddPacketFeedback(0, 1000, 30, 40);
    AddPacketFeedback(0, 1500, 40, 50);
    // Expected send rate = 800 kbps, expected receive rate = 900 kbps.

    EXPECT_NEAR(measured_bitrate_->bps(), 800000, 10);
}

MY_TEST_F(ProbeBitrateEstimatorTest, IgnoreSizeFirstReceivePacket) {
    AddPacketFeedback(0, 1500, 0, 10);
    AddPacketFeedback(0, 1000, 10, 20);
    AddPacketFeedback(0, 1000, 20, 30);
    AddPacketFeedback(0, 1000, 30, 40);
    // Expected send rate = 933 kbps, expected receive rate = 800 kbps.

    EXPECT_NEAR(measured_bitrate_->bps(), kTargetUtilizationFraction * 800000,
                10);
}

MY_TEST_F(ProbeBitrateEstimatorTest, NoLastEstimatedBitrateBps) {
    EXPECT_FALSE(probe_bitrate_estimator_.Estimate());
}

MY_TEST_F(ProbeBitrateEstimatorTest, FetchLastEstimatedBitrateBps) {
    AddPacketFeedback(0, 1000, 0, 10);
    AddPacketFeedback(0, 1000, 10, 20);
    AddPacketFeedback(0, 1000, 20, 30);
    AddPacketFeedback(0, 1000, 30, 40);

    auto estimated_bitrate = probe_bitrate_estimator_.Estimate();
    EXPECT_TRUE(estimated_bitrate);
    EXPECT_NEAR(estimated_bitrate->bps(), 800000, 10);
    EXPECT_FALSE(probe_bitrate_estimator_.Estimate());
}

} // namespace test    
} // namespace naivertc
