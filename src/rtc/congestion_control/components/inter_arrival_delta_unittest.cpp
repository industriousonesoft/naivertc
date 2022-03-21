#include "rtc/congestion_control/components/inter_arrival_delta.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"
#include "testing/simulated_clock.hpp"

namespace naivertc {
namespace test {
namespace {

constexpr TimeDelta kSendTimeGroupSpan = TimeDelta::Millis(5); // 5ms
constexpr TimeDelta kBurstDeltaThreshold = TimeDelta::Millis(5); // 5ms

} // namespace

class T(InterArrivalDeltaTest) : public ::testing::Test {
public:
    T(InterArrivalDeltaTest)()
        : clock_(1000),
          inter_arrival_(kSendTimeGroupSpan) {}

protected:
    SimulatedClock clock_;
    InterArrivalDelta inter_arrival_;
};

MY_TEST_F(InterArrivalDeltaTest, ComputeDeltas) {
    const auto kPropagationDelay = TimeDelta::Millis(70);
    const auto kRtt = TimeDelta::Millis(150); // 2*kPropagationDelay + 10ms
    const size_t kPacketSize = 1000;

    auto send_recv_report_packet = [&]() -> std::optional<InterArrivalDelta::Result> {
        auto send_time = clock_.CurrentTime();
        auto arrival_time = send_time + kPropagationDelay;
        auto report_time = send_time + kRtt;

        return inter_arrival_.ComputeDeltas(send_time,
                                            arrival_time,
                                            report_time,
                                            kPacketSize);
    };

    auto ret = send_recv_report_packet();
    // The first packet of the first group.
    EXPECT_FALSE(ret.has_value());

    // Advance time within the first group.
    clock_.AdvanceTimeMs(5);
    auto first_group_last_send_time = clock_.CurrentTime();
    auto first_group_last_arrival_time = clock_.CurrentTime();
    ret = send_recv_report_packet();

    // Still in the first group.
    EXPECT_FALSE(ret.has_value());

    // Detect the second packet group
    clock_.AdvanceTimeMs(1);
    ret = send_recv_report_packet();

    // Need two completed group to calculate deltas at least,
    // the new group is not completed yet.
    EXPECT_FALSE(ret.has_value());

    clock_.AdvanceTimeMs(2);
    auto second_group_last_send_time = clock_.CurrentTime();
    auto second_group_last_arrival_time = clock_.CurrentTime();
    ret = send_recv_report_packet();

    // Detect the third packet group.
    clock_.AdvanceTimeMs(5);
    ret = send_recv_report_packet();

    // We can calculate deltas now.
    EXPECT_TRUE(ret.has_value());

    EXPECT_EQ(ret->send_time_delta, second_group_last_send_time - first_group_last_send_time);
    EXPECT_EQ(ret->arrival_time_delta, second_group_last_arrival_time - first_group_last_arrival_time);
    
}

MY_TEST_F(InterArrivalDeltaTest, DetectABurst) {
    const auto kPropagationDelay = TimeDelta::Millis(100);
    const auto kArrivalTimeOffset = TimeDelta::Millis(50);
    const size_t kPacketSize = 1000;
    
    // Three packet groups
    auto send_time_1 = clock_.CurrentTime();
    auto send_time_2 = send_time_1 + 2 * kSendTimeGroupSpan;
    auto send_time_3 = send_time_2 + 2 * kSendTimeGroupSpan;
    auto send_time_4 = send_time_3 + 2 * kSendTimeGroupSpan;

    auto arrival_time_1 = send_time_1 + kPropagationDelay;
    auto arrival_time_2 = send_time_2 + kPropagationDelay;
    auto arrival_time_3 = send_time_3 + kPropagationDelay;

    // A burst
    auto arrival_time_4 = arrival_time_3 + kBurstDeltaThreshold - TimeDelta::Millis(1);

    // The first packet group.
    auto ret = inter_arrival_.ComputeDeltas(send_time_1,
                                            arrival_time_1,
                                            arrival_time_1 + kArrivalTimeOffset,
                                            kPacketSize);
    // At least two completed group needed to calculate the deltas.
    EXPECT_FALSE(ret.has_value());

    // The second packet group.
    ret = inter_arrival_.ComputeDeltas(send_time_2,
                                        arrival_time_2,
                                        arrival_time_2 + kArrivalTimeOffset,
                                        kPacketSize);

    // At least two completed group needed to calculate the deltas.
    EXPECT_FALSE(ret.has_value());

    // The third packet group.
    ret = inter_arrival_.ComputeDeltas(send_time_3,
                                       arrival_time_3,
                                       arrival_time_3 + kArrivalTimeOffset,
                                       kPacketSize);
    // We can calculate the deltas now.
    EXPECT_TRUE(ret.has_value());

    // The burst group will not be detected as a packet group.
    ret = inter_arrival_.ComputeDeltas(send_time_4,
                                       arrival_time_4,
                                       arrival_time_4 + kArrivalTimeOffset,
                                       kPacketSize);
    // The burst packets belongs to the current packet group.
    EXPECT_FALSE(ret.has_value());
}


MY_TEST_F(InterArrivalDeltaTest, ResetAsArrivalTimeClockHasChanged) {
    const auto kPropagationDelay = TimeDelta::Millis(70);
    const auto kRtt = TimeDelta::Millis(150); // 2*kPropagationDelay + 10ms
    const size_t kPacketSize = 1000;
    const auto kArrivalTimeOffset = InterArrivalDelta::kArrivalTimeOffsetThreshold;

    auto send_recv_report_packet = [&](TimeDelta arrival_time_offset) -> std::optional<InterArrivalDelta::Result> {
        auto send_time = clock_.CurrentTime();
        auto arrival_time = send_time + kPropagationDelay + arrival_time_offset;
        auto report_time = send_time + kRtt;

        return inter_arrival_.ComputeDeltas(send_time,
                                            arrival_time,
                                            report_time,
                                            kPacketSize);
    };

    auto ret = send_recv_report_packet(TimeDelta::Zero());
    // The first packet of the first group.
    EXPECT_FALSE(ret.has_value());

    // Advance time within the first group.
    clock_.AdvanceTimeMs(5);
    ret = send_recv_report_packet(TimeDelta::Zero());

    // Still in the first group.
    EXPECT_FALSE(ret.has_value());

    // The arrival time clock offset has changed

    // Detect the second packet group
    clock_.AdvanceTimeMs(1);
    ret = send_recv_report_packet(kArrivalTimeOffset);

    // Need two completed group to calculate deltas at least,
    // the new group is not completed yet.
    EXPECT_FALSE(ret.has_value());

    // Detect the third packet group.
    clock_.AdvanceTimeMs(6);
    ret = send_recv_report_packet(kArrivalTimeOffset);

    // Reset as the arrival time clock has changed.
    EXPECT_FALSE(ret.has_value());

}

} // namespace test
} // namespace naivertc