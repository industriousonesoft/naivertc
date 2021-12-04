#include "rtc/congestion_controller/goog_cc/trendline_estimator.hpp"

#include <vector>

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {

class PacketTimeGenerator {
public:
    PacketTimeGenerator(int64_t initial_clock, double packet_interval_ms) 
        : initial_clock_(initial_clock),
          packet_interval_ms_(packet_interval_ms),
          packet_count_(0) {}

    int64_t operator()() {
        return initial_clock_ + packet_interval_ms_ * packet_count_++;
    }

private:
    const int64_t initial_clock_;
    const double packet_interval_ms_;
    size_t packet_count_;
};

class T(TrendlineEstimatorTest) : public testing::Test {
public:
    T(TrendlineEstimatorTest)() 
        : send_times(kPacketCount),
          recv_times(kPacketCount),
          packet_sizes(kPacketCount),
          estimator(config),
          packet_index(1) {
        std::fill(packet_sizes.begin(), packet_sizes.end(), kPacketSizeBytes);
    }

    void RunTestUntilStateChange() {
        ASSERT_EQ(send_times.size(), kPacketCount);
        ASSERT_EQ(recv_times.size(), kPacketCount);
        ASSERT_EQ(packet_sizes.size(), kPacketCount);
        ASSERT_GE(packet_index, 1);
        ASSERT_LT(packet_index, kPacketCount);
        
        auto initial_state = estimator.State();
        for (; packet_index < kPacketCount; ++packet_index) {
            double recv_delta = recv_times[packet_index] - recv_times[packet_index - 1];
            double send_delta = send_times[packet_index] - send_times[packet_index - 1];
            estimator.Update(recv_delta, send_delta, send_times[packet_index], recv_times[packet_index], packet_sizes[packet_index], true);
            if (estimator.State() != initial_state) {
                return;
            }
        }
    }
protected:
    const size_t kPacketCount = 25;
    const size_t kPacketSizeBytes = 1200;
    std::vector<int64_t> send_times;
    std::vector<int64_t> recv_times;
    std::vector<size_t> packet_sizes;
    TrendlineEstimator::Configuration config;
    TrendlineEstimator estimator;
    size_t packet_index;
};

} // namespace

// MY_TEST_F(TrendlineEstimatorTest, Normal) {
//     PacketTimeGenerator send_time_generator(123456789 /*initial clock*/,
//                                             20 /*20 ms between sent packets*/);
//     std::generate(send_times.begin(), send_times.end(), send_time_generator);

//     PacketTimeGenerator recv_time_generator(987654321 /*initial clock*/,
//                                             20 /*delivered at the same pace*/);
//     std::generate(recv_times.begin(), recv_times.end(), recv_time_generator);

//     EXPECT_EQ(estimator.State(), BandwidthUsage::NORMAL);
//     RunTestUntilStateChange();
//     EXPECT_EQ(estimator.State(), BandwidthUsage::NORMAL);
//     EXPECT_EQ(packet_index, kPacketCount);  // All packets processed
// }

MY_TEST_F(TrendlineEstimatorTest, Overusing) {
    PacketTimeGenerator send_time_generator(123456789 /*initial clock*/,
                                            20 /*20 ms between sent packets*/);
    std::generate(send_times.begin(), send_times.end(), send_time_generator);

    PacketTimeGenerator recv_time_generator(987654321 /*initial clock*/,
                                            1.1 * 20 /*10% slower delivery*/);
    std::generate(recv_times.begin(), recv_times.end(), recv_time_generator);
    
    EXPECT_EQ(estimator.State(), BandwidthUsage::NORMAL);
    RunTestUntilStateChange();
    EXPECT_EQ(estimator.State(), BandwidthUsage::OVERUSING);
    RunTestUntilStateChange();
    EXPECT_EQ(estimator.State(), BandwidthUsage::OVERUSING);
    EXPECT_EQ(packet_index, kPacketCount);  // All packets processed
}

// MY_TEST_F(TrendlineEstimatorTest, Underusing) {
//     PacketTimeGenerator send_time_generator(123456789 /*initial clock*/,
//                                             20 /*20 ms between sent packets*/);
//     std::generate(send_times.begin(), send_times.end(), send_time_generator);

//     PacketTimeGenerator recv_time_generator(987654321 /*initial clock*/,
//                                             0.85 * 20 /*15% faster delivery*/);
//     std::generate(recv_times.begin(), recv_times.end(), recv_time_generator);

//     EXPECT_EQ(estimator.State(), BandwidthUsage::NORMAL);
//     RunTestUntilStateChange();
//     EXPECT_EQ(estimator.State(), BandwidthUsage::UNDERUSING);
//     RunTestUntilStateChange();
//     EXPECT_EQ(estimator.State(), BandwidthUsage::UNDERUSING);
//     EXPECT_EQ(packet_index, kPacketCount);  // All packets processed
// }

// MY_TEST_F(TrendlineEstimatorTest, IncludesSmallPacketsByDefault) {
//     PacketTimeGenerator send_time_generator(123456789 /*initial clock*/,
//                                             20 /*20 ms between sent packets*/);
//     std::generate(send_times.begin(), send_times.end(), send_time_generator);

//     PacketTimeGenerator recv_time_generator(987654321 /*initial clock*/,
//                                             1.1 * 20 /*10% slower delivery*/);
//     std::generate(recv_times.begin(), recv_times.end(), recv_time_generator);

//     std::fill(packet_sizes.begin(), packet_sizes.end(), 100);

//     EXPECT_EQ(estimator.State(), BandwidthUsage::NORMAL);
//     RunTestUntilStateChange();
//     EXPECT_EQ(estimator.State(), BandwidthUsage::OVERUSING);
//     RunTestUntilStateChange();
//     EXPECT_EQ(estimator.State(), BandwidthUsage::OVERUSING);
//     EXPECT_EQ(packet_index, kPacketCount);  // All packets processed
// }
    
} // namespace test
} // namespace naivertc
