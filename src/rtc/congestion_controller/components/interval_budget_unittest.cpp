#include "rtc/congestion_controller/components/interval_budget.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 1
#include "testing/defines.hpp"

namespace naivertc {
namespace test {
namespace {
    
constexpr TimeDelta kBudgetWindow = TimeDelta::Millis(500);
constexpr DataRate kTargetBitrate = DataRate::KilobitsPerSec(100);

size_t IntervalBytes(const DataRate& bitrate, const TimeDelta& interval) {
    return static_cast<size_t>((bitrate.kbps() * interval.ms()) / 8);
}

TimeDelta IntervalTime(const DataRate& bitrate, size_t bytes) {
    return TimeDelta::Millis(static_cast<int64_t>((bytes * 8) / bitrate.kbps<double>()));
}

} // namespace

class T(IntervalBudgetTest) : public ::testing::TestWithParam<bool> {
public:
    T(IntervalBudgetTest)() 
        : interval_budget_(kTargetBitrate, /*can_build_up_from_underuse=*/GetParam()) {}

protected:
    IntervalBudget interval_budget_;
};

MY_INSTANTIATE_TEST_SUITE_P(WithAndWithoutCanBuildUpFromUnderuse, IntervalBudgetTest, ::testing::Bool());

MY_TEST_P(IntervalBudgetTest, InitialConfiguration) {
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), 0.0);
    EXPECT_EQ(interval_budget_.bytes_remaining(), 0u);
}

MY_TEST_P(IntervalBudgetTest, Underuse) {
    auto interval_time = TimeDelta::Millis(50);
    // Increase budget
    interval_budget_.IncreaseBudget(interval_time);

    // Consume budget but still in underuse
    int consumed_bytes = 600; // 48ms
    auto consumed_time = IntervalTime(kTargetBitrate, consumed_bytes);
    interval_budget_.ConsumeBudget(consumed_bytes);

    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), (interval_time - consumed_time) / kBudgetWindow) 
                    << consumed_time.ms();
    EXPECT_GT(interval_budget_.bytes_remaining(), 0u);
    EXPECT_EQ(interval_budget_.bytes_remaining(), IntervalBytes(kTargetBitrate, interval_time - consumed_time));

    // Continue consuming 2ms
    interval_budget_.ConsumeBudget(25);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), 0);
    EXPECT_EQ(interval_budget_.bytes_remaining(), 0u);
}

MY_TEST_P(IntervalBudgetTest, DontUnderuseMoreThanMaxWindow) {
    auto interval_time = TimeDelta::Millis(1000);
    interval_budget_.IncreaseBudget(interval_time);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), 1.0);
    EXPECT_EQ(interval_budget_.bytes_remaining(),
              IntervalBytes(kTargetBitrate, kBudgetWindow));
}

MY_TEST_P(IntervalBudgetTest, DontUnderuseMoreThanMaxWindowWhenChangeBitrate) {
    auto interval_time = kBudgetWindow / 2;
    interval_budget_.IncreaseBudget(interval_time);
    interval_budget_.set_target_bitrate(kTargetBitrate / 10);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), 1.0);
    EXPECT_EQ(interval_budget_.bytes_remaining(),
              IntervalBytes(kTargetBitrate / 10, kBudgetWindow));
}

MY_TEST_P(IntervalBudgetTest, BalanceChangeOnBitrateChange) {
    auto interval_time = kBudgetWindow;
    interval_budget_.IncreaseBudget(interval_time);
    interval_budget_.set_target_bitrate(kTargetBitrate * 2);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), 0.5);
    EXPECT_EQ(interval_budget_.bytes_remaining(),
              IntervalBytes(kTargetBitrate, kBudgetWindow));
}

MY_TEST_P(IntervalBudgetTest, Overuse) {
    auto overuse_time = TimeDelta::Millis(50);
    int consumed_bytes = IntervalBytes(kTargetBitrate, overuse_time);
    interval_budget_.ConsumeBudget(consumed_bytes);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(),
                     overuse_time.ms<double>() / -kBudgetWindow.ms<double>());
    EXPECT_EQ(interval_budget_.bytes_remaining(), 0u);
}

MY_TEST_P(IntervalBudgetTest, DontOveruseMoreThanMaxWindow) {
    auto overuse_time = TimeDelta::Millis(1000);
    int consumed_bytes = IntervalBytes(kTargetBitrate, overuse_time);
    interval_budget_.ConsumeBudget(consumed_bytes);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(), -1.0);
    EXPECT_EQ(interval_budget_.bytes_remaining(), 0u);
}

MY_TEST_P(IntervalBudgetTest, CanBuildUpFromUnderuseWhenConfigured) {
    auto interval_time = TimeDelta::Millis(50);
    // Increase budget
    interval_budget_.IncreaseBudget(interval_time);
    EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(),
                    interval_time.ms<double>() / kBudgetWindow.ms<double>());
    EXPECT_EQ(interval_budget_.bytes_remaining(),
              IntervalBytes(kTargetBitrate, interval_time));

    // Consume budget but still in underuse
    int consumed_bytes = 100;
    auto consumed_time = IntervalTime(kTargetBitrate, consumed_bytes);
    interval_budget_.ConsumeBudget(consumed_bytes);

    // Increase budget
    interval_budget_.IncreaseBudget(interval_time);
    
    if (GetParam()) {
        // Build up budget from underuse.
        EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(),
                         (2 * interval_time - consumed_time).ms<double>() / kBudgetWindow.ms<double>());
        EXPECT_EQ(interval_budget_.bytes_remaining(),
                  IntervalBytes(kTargetBitrate, 2 * interval_time) - consumed_bytes);
    } else {
        EXPECT_DOUBLE_EQ(interval_budget_.budget_ratio(),
                         interval_time.ms<double>() / kBudgetWindow.ms<double>());
        EXPECT_EQ(interval_budget_.bytes_remaining(),
                  IntervalBytes(kTargetBitrate, interval_time));
    }
    
}
    
} // namespace test
} // namespace naivertc
